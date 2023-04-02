#pragma once

#include "bee/util.hpp"
#include "scheduler_context.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <type_traits>
#include <vector>

namespace async {

template <std::move_constructible T> struct DeferredImpl;
template <std::move_constructible T> struct Deferred;

template <std::move_constructible T>
struct Ivar : public std::enable_shared_from_this<Ivar<T>> {
 public:
  using ptr = std::shared_ptr<Ivar>;
  using element_type = T;

  Ivar() {}

  ~Ivar() {}

  Ivar(const T& value) : _value(value) {}

  static ptr create() { return std::make_shared<Ivar<T>>(); }

  template <std::convertible_to<T> U> static ptr create_with_value(U&& value)
  {
    Ivar<T>::ptr ivar = std::make_shared<Ivar<T>>();
    ivar->resolve(std::forward<U>(value));
    return ivar;
  }

  template <std::convertible_to<T> P> void resolve(P&& value)
  {
    _set_value(std::forward<P>(value));
    _maybe_schedule();
  }

  template <std::invocable<T> F> void on_resolved(F&& callback)
  {
    if (_listener) { assert(false && "Ivar already has a listener"); }
    _listener = std::forward<F>(callback);
    _maybe_schedule();
  }

  static Deferred<T> value(const ptr& ivar) { return Deferred<T>(ivar); }

  bool is_resolved() { return _is_resolved; }

 private:
  template <std::convertible_to<T> P> void _set_value(P&& value)
  {
    if (_dead || _value.has_value()) {
      assert(false && "Ivar already resolved");
    }
    _value.emplace(std::forward<P>(value));
    _is_resolved = true;
  }

  void _maybe_schedule()
  {
    if (_listener && _value.has_value() && !_dead) {
      schedule([ptr = this->shared_from_this()]() { ptr->_deliver(); });
      _dead = true;
    }
  }

  void _deliver()
  {
    _listener(std::move(*_value));
    _listener = nullptr;
    _value = std::nullopt;
  }

  std::function<void(T&& value)> _listener;
  std::optional<T> _value;
  bool _dead = false;
  bool _is_resolved = false;
};

template <class T> Deferred<T> ivar_value(const std::shared_ptr<Ivar<T>>& ivar)
{
  return Ivar<T>::value(ivar);
}

template <class R> static const auto& get_def_impl(const R& d)
{
  if constexpr (R::is_impl) {
    return d;
  } else {
    return d.impl();
  }
}

template <class R>
static const typename DeferredImpl<R>::ptr& get_def_impl(
  const DeferredImpl<R>& d)
{
  return d;
}

////////////////////////////////////////////////////////////////////////////////
// is_deferred
//

template <std::move_constructible T> struct Deferred;

template <class T> struct is_deferred_t;

template <class T> struct is_deferred_t<Deferred<T>> {
  static constexpr bool value = true;
};

template <class T> struct is_deferred_t {
  static constexpr bool value = false;
};

template <class T>
concept deferred = is_deferred_t<T>::value;

template <class F, class T>
concept deferred_bind_fn = requires(F f, T t) {
                             {
                               f(t)
                               } -> deferred;
                           };

////////////////////////////////////////////////////////////////////////////////
// DeferredImpl
//

template <std::move_constructible T> struct DeferredImpl {
 public:
  using ptr = std::shared_ptr<DeferredImpl>;
  using element_type = T;

  DeferredImpl(const std::shared_ptr<Ivar<T>>& ivar) : _ivar(ivar) {}
  DeferredImpl(std::shared_ptr<Ivar<T>>&& ivar) : _ivar(std::move(ivar)) {}

  explicit DeferredImpl(const T& value)
      : _ivar(Ivar<T>::create_with_value(value))
  {}

  static ptr create(const typename Ivar<T>::ptr& ivar)
  {
    return std::make_shared<DeferredImpl<T>>(ivar);
  }

  static ptr create(typename Ivar<T>::ptr&& ivar)
  {
    return std::make_shared<DeferredImpl<T>>(std::move(ivar));
  }

  bool is_resolved() const { return _ivar->is_resolved(); }

  template <class U> static ptr create_with_value(U&& v)
  {
    std::shared_ptr<Ivar<T>> ivar = Ivar<T>::create_with_value(v);
    return std::make_shared<DeferredImpl<T>>(std::move(ivar));
  }

  template <std::invocable<T> F> auto map(F&& callback)
  {
    using P = std::invoke_result_t<F, T>;
    auto out = Ivar<P>::create();

    _ivar->on_resolved(
      [callback = std::move(callback), out, ivar = _ivar](T&& value) {
        out->resolve(callback(std::move(value)));
      });

    return ivar_value(out);
  }

  template <std::invocable<T> F> auto bind(F&& callback)
  {
    using return_type = typename std::invoke_result_t<F, T>;
    using P = typename return_type::element_type;
    auto ivar = Ivar<P>::create();

    iter([callback = std::forward<F>(callback), ivar](T&& value) mutable {
      auto b = callback(std::move(value));

      get_def_impl(b)->iter(
        [ivar](P&& value) { ivar->resolve(std::move(value)); });
    });

    return ivar_value(ivar);
  }

  template <class F> void iter(F&& callback)
  {
    _ivar->on_resolved(std::forward<F>(callback));
  }

  T& value() { return _ivar->value(); }
  const T& value() const { return _ivar->value(); }

  static ptr never()
  {
    auto ivar = Ivar<T>::create();
    return create(ivar);
  }

 private:
  typename Ivar<T>::ptr _ivar;
};

////////////////////////////////////////////////////////////////////////////////
// Never
//

struct never_t {};

constexpr never_t never;

////////////////////////////////////////////////////////////////////////////////
// Deferred
//

template <std::move_constructible T> struct Deferred {
 public:
  using element_type = T;
  static constexpr bool is_impl = false;

  Deferred(const std::shared_ptr<Ivar<T>>& v) : _d(DeferredImpl<T>::create(v))
  {}
  Deferred(std::shared_ptr<Ivar<T>>&& v)
      : _d(DeferredImpl<T>::create(std::move(v)))
  {}

  Deferred(const Deferred& o) : _d(o._d) {}
  Deferred(Deferred&& o) : _d(std::move(o._d)) {}

  Deferred(const never_t&) : _d(DeferredImpl<T>::never()) {}

  template <class U>
    requires std::constructible_from<T, U>
  Deferred(const Deferred<U>& v)
      : Deferred(v.map([](U&& v) -> T { return std::move(v); }))
  {}

  template <class U>
    requires std::constructible_from<T, U>
  Deferred(Deferred<U>&& v)
      : Deferred(v.map([](U&& v) -> T { return std::move(v); }))
  {}

  template <class U>
    requires std::constructible_from<T, U>
  Deferred(const std::shared_ptr<Ivar<U>>& v) : Deferred(ivar_value(v))
  {}

  template <class U>
    requires std::constructible_from<T, U>
  Deferred(std::shared_ptr<Ivar<U>>&& v) : Deferred(ivar_value(v))
  {}

  template <class U>
    requires std::constructible_from<T, U>
  Deferred(U&& v) : _d(DeferredImpl<T>::create_with_value(std::forward<U>(v)))
  {}

  template <class F> auto map(F&& callback) const
  {
    static_assert(
      std::invocable<F, T>,
      "map function doesn't accept Deferred inner value type");
    using P = std::invoke_result_t<F, T>;
    static_assert(!deferred<P>, "map function must not return a deferred");
    return _d->map(std::forward<F>(callback));
  }

  template <class F> auto bind(F&& callback) const
  {
    static_assert(
      std::invocable<F, T>,
      "bind function doesn't accept Deferred inner value type");
    using P = std::invoke_result_t<F, T>;
    static_assert(deferred<P>, "bind function must return a deferred");
    return _d->bind(std::forward<F>(callback));
  }

  template <class F> void iter(F&& callback) const
  {
    static_assert(
      std::invocable<F, T>,
      "iter function doesn't accept Deferred inner value type");
    _d->iter(std::forward<F>(callback));
  }

  bool is_resolved() const { return _d->is_resolved(); }

  const typename DeferredImpl<T>::ptr& impl() const { return _d; }

 private:
  typename DeferredImpl<T>::ptr _d;
};

////////////////////////////////////////////////////////////////////////////////
// Helpers
//

namespace __internal {

template <class T>
struct RepeatRunner : public std::enable_shared_from_this<RepeatRunner<T>> {
 public:
  template <class F>
  RepeatRunner(int times, F&& f)
      : _counter(times), _callback(std::forward<F>(f))
  {}

  void start_worker() { _workers.push(run_worker()); }

  Deferred<std::vector<T>> wait()
  {
    auto ptr = this->shared_from_this();
    return _wait_all().map(
      [ptr](const bee::Unit&) { return std::move(ptr->_results); });
  }

 private:
  Deferred<bee::Unit> run_worker()
  {
    if (_counter == 0) return bee::unit;
    _counter--;
    auto ptr = this->shared_from_this();
    return _callback().bind([ptr](T&& result) {
      ptr->_results.push_back(std::move(result));
      return ptr->run_worker();
    });
  };

  Deferred<bee::Unit> _wait_all()
  {
    if (_workers.empty()) { return bee::unit; }
    auto d = _workers.front();
    _workers.pop();
    auto ptr = this->shared_from_this();
    return d.bind([ptr](const bee::Unit&) { return ptr->_wait_all(); });
  }

  int _counter;
  std::vector<T> _results;
  std::queue<Deferred<bee::Unit>> _workers;
  std::function<Deferred<T>()> _callback;
};

template <class T, class F>
struct IterVectorState
    : public std::enable_shared_from_this<IterVectorState<T, F>> {
 public:
  IterVectorState(std::vector<T>&& v, F&& f)
      : _v(std::move(v)), _f(std::move(f)), _it(_v.begin())
  {}

  Deferred<bee::Unit> next()
  {
    if (_it == _v.end()) { return bee::unit; }
    return _f(*(_it++)).bind(
      [ptr = this->shared_from_this()]() { ptr->next(); });
  }

 private:
  std::vector<T> _v;
  F _f;
  typename std::vector<T>::iterator _it;
};

template <class T>
struct WaitAllState : public std::enable_shared_from_this<WaitAllState<T>> {
  using v_t = std::vector<Deferred<T>>;

 public:
  WaitAllState(const v_t& v) : _v(v), _it(_v.begin()) {}

  Deferred<std::vector<T>> next()
  {
    if (_it == _v.end()) { return std::move(_result); }
    return (_it++)->bind([ptr = this->shared_from_this()](T&& v) {
      ptr->_result.push_back(v);
      return ptr->next();
    });
  }

 private:
  v_t _v;
  typename v_t::iterator _it;

  std::vector<T> _result;
};

template <
  class F,
  class T = typename std::invoke_result_t<F>::element_type::value_type>
inline void repeat_until_impl(typename Ivar<T>::ptr&& ivar, F&& f)
{
  f().iter([f = std::move(f),
            ivar = std::move(ivar)](std::optional<T>&& result) mutable {
    if (result.has_value()) {
      ivar->resolve(std::move(*result));
    } else {
      return repeat_until_impl(std::move(ivar), std::move(f));
    }
  });
}

template <class T> T copy(const T& value) { return value; }

} // namespace __internal

////////////////////////////////////////////////////////////////////////////////
// Utilities
//

template <class T, class U>
Deferred<std::pair<T, U>> wait_all(const Deferred<T>& d1, const Deferred<U>& d2)
{
  return d1.bind([d2](T&& t) {
    return d2.map([t = std::move(t)](U&& u) {
      return std::pair<T, U>(std::move(t), std::move(u));
    });
  });
}

template <class T>
Deferred<std::vector<T>> wait_all(const std::vector<Deferred<T>>& d)
{
  auto state = std::make_shared<__internal::WaitAllState<T>>(d);
  return state->next();
}

template <class T, class U, class F>
void iter(const Deferred<T>& d1, const Deferred<U>& d2, F&& callback)
{
  wait(d1, d2).iter(
    [callback = std::forward<F>(callback)](std::pair<T, U>& result) {
      callback(std::move(result.first), std::move(result.second));
    });
}

template <class T, std::invocable<T> F>
Deferred<bee::Unit> iter_vector(std::vector<T>&& v, F&& callback)
{
  auto state = make_shared<T, F>(std::move(v), std::move(callback));
  return state->next();
}

Deferred<bee::OrError<bee::Unit>> repeat(
  int times, const std::function<Deferred<bee::OrError<bee::Unit>>()>& f);

template <class F, class T = typename std::invoke_result_t<F>::element_type>
inline Deferred<std::vector<T>> repeat_parallel(
  int times, int concurrency, F&& f)
{
  auto runner =
    std::make_shared<__internal::RepeatRunner<T>>(times, std::forward<F>(f));
  for (int i = 0; i < concurrency; i++) { runner->start_worker(); }
  return runner->wait();
}

template <
  class F,
  class T = typename std::invoke_result_t<F>::element_type::value_type>
inline Deferred<T> repeat_until(F&& f)
{
  auto ivar = Ivar<T>::create();
  __internal::repeat_until_impl(__internal::copy(ivar), std::move(f));
  return ivar_value(ivar);
}

Deferred<bee::Unit> after(const bee::Span& span);

} // namespace async
