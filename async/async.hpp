#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "scheduler_context.hpp"

#include "bee/unit.hpp"
#include "bee/util.hpp"

namespace async {

template <class T>
concept deferable_value = std::is_void_v<T> || std::is_move_constructible_v<T>;

template <deferable_value T> struct DeferredImpl;
template <deferable_value T> struct Deferred;

namespace details {

template <class F, class T> struct is_invocable;

template <class F> struct is_invocable<F, void> {
  constexpr static bool value = std::invocable<F>;
};

template <class F, class T> struct is_invocable {
  constexpr static bool value = std::invocable<F, T>;
};

template <class F, class T>
concept invocable = is_invocable<F, T>::value;

template <class T, class... Args> struct is_constructible;

template <> struct is_constructible<void> {
  constexpr static bool value = true;
};

template <> struct is_constructible<void, void> {
  constexpr static bool value = true;
};

template <class T, class... Args> struct is_constructible {
  constexpr static bool value = std::is_constructible_v<T, Args...>;
};

template <class T, class... Args>
concept constructible_from = is_constructible<T, Args...>::value;

template <class F, class... Args> struct invoke_result;

template <class F> struct invoke_result<F, void> {
  using type = std::invoke_result_t<F>;
};

template <class F, class... Args> struct invoke_result {
  using type = std::invoke_result_t<F, Args...>;
};

template <class F, class... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;

} // namespace details

template <deferable_value T = void>
struct Ivar : public std::enable_shared_from_this<Ivar<T>> {
 private:
  template <class U> struct make_listener_type;
  template <> struct make_listener_type<void> {
    using type = std::function<void()>;
  };
  template <class U> struct make_listener_type {
    using type = std::function<void(U&&)>;
  };

  struct filled_t {};

 public:
  using ptr = std::shared_ptr<Ivar>;
  using value_type = T;
  using rvalue_type = std::add_rvalue_reference_t<value_type>;
  using lvalue_type = std::add_lvalue_reference_t<value_type>;

  using listener_t = typename make_listener_type<T>::type;

  Ivar() {}
  ~Ivar() {}

  template <class... Args>
  Ivar(filled_t, Args&&... args) : _value(std::forward<Args>(args)...)
  {
    if constexpr (std::is_void_v<T>) {
      // Make sure _value is not empty if T is void
      _value.emplace();
    }
  }

  static ptr create() { return std::make_shared<Ivar<T>>(); }

  template <class... Args>
    requires details::constructible_from<T, Args...>
  static ptr create_with_value(Args&&... args)
  {
    return std::make_shared<Ivar<T>>(filled_t{}, std::forward<Args>(args)...);
  }

  template <class... Args>
    requires details::constructible_from<T, Args...>
  void fill(Args&&... args)
  {
    _set_value(std::forward<Args>(args)...);
    _maybe_schedule();
  }

  template <details::invocable<T> F> void on_determined(F&& callback)
  {
    if (_listener) { assert(false && "Ivar already has a listener"); }
    _listener = std::forward<F>(callback);
    _maybe_schedule();
  }

  static Deferred<T> value(const ptr& ivar) { return Deferred<T>(ivar); }

  lvalue_type value() &
  {
    if (!_value.has_value()) { assert(false && "Ivar is not determined"); }
    if constexpr (!std::is_void_v<T>) { return *_value; }
  }

  rvalue_type value() &&
  {
    if (!_value.has_value()) { assert(false && "Ivar is not determined"); }
    if constexpr (!std::is_void_v<T>) { return std::move(*_value); }
  }

  bool is_determined() const { return _is_determined; }

 private:
  template <class... Args> void _set_value(Args&&... args)
  {
    if (_dead || _value.has_value()) {
      assert(false && "Ivar already determined");
    }
    _value.emplace(std::forward<Args>(args)...);
    _is_determined = true;
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
    if constexpr (std::is_void_v<T>) {
      _listener();
    } else {
      _listener(std::move(*_value));
    }
    _listener = nullptr;
  }

  listener_t _listener;
  std::optional<bee::unit_if_void_t<T>> _value;
  bool _dead = false;
  bool _is_determined = false;
};

template <deferable_value T>
Deferred<T> ivar_value(const std::shared_ptr<Ivar<T>>& ivar)
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

template <class T> struct is_deferred;

template <class T> struct is_deferred<Deferred<T>> {
  static constexpr bool value = true;
};

template <class T> struct is_deferred {
  static constexpr bool value = false;
};

template <class T> constexpr bool is_deferred_v = is_deferred<T>::value;

template <class T>
concept deferred = is_deferred_v<T>;

template <class F, class T>
concept deferred_bind_fn = requires(F f, T t) {
  {
    f(t)
  } -> deferred;
};

////////////////////////////////////////////////////////////////////////////////
// DeferredImpl
//

template <deferable_value T = void> struct DeferredImpl {
 public:
  using ptr = std::shared_ptr<DeferredImpl>;

  using value_type = T;
  using lvalue_type = std::add_lvalue_reference_t<value_type>;
  using rvalue_type = std::add_rvalue_reference_t<value_type>;

  using const_value_type = const T;
  using const_lvalue_type = std::add_lvalue_reference_t<const_value_type>;

  DeferredImpl(const std::shared_ptr<Ivar<T>>& ivar) : _ivar(ivar) {}
  DeferredImpl(std::shared_ptr<Ivar<T>>&& ivar) : _ivar(std::move(ivar)) {}

  template <std::convertible_to<T> U>
  explicit DeferredImpl(U&& value)
      : _ivar(Ivar<T>::create_with_value(std::forward<U>(value)))
  {}

  static ptr create(const typename Ivar<T>::ptr& ivar)
  {
    return std::make_shared<DeferredImpl<T>>(ivar);
  }

  static ptr create(typename Ivar<T>::ptr&& ivar)
  {
    return std::make_shared<DeferredImpl<T>>(std::move(ivar));
  }

  bool is_determined() const { return _ivar->is_determined(); }

  template <class... Args>
    requires details::constructible_from<T, Args...>
  static ptr create_with_value(Args&&... args)
  {
    std::shared_ptr<Ivar<T>> ivar =
      Ivar<T>::create_with_value(std::forward<Args>(args)...);
    return std::make_shared<DeferredImpl<T>>(std::move(ivar));
  }

  template <details::invocable<T> F> auto map(F&& callback)
  {
    using P = details::invoke_result_t<F, T>;
    auto out = Ivar<P>::create();

    _ivar->on_determined(
      [callback = std::move(callback), out, ivar = _ivar]<class... Args>(
        Args&&... args) { out->fill(callback(std::forward<Args>(args)...)); });

    return ivar_value(out);
  }

  template <details::invocable<T> F> auto bind(F&& callback)
  {
    using return_type = typename details::invoke_result_t<F, T>;
    using P = typename return_type::value_type;
    auto ivar = Ivar<P>::create();

    iter([callback = std::forward<F>(callback),
          ivar]<class... Args>(Args&&... args) mutable {
      auto b = callback(std::forward<Args>(args)...);

      get_def_impl(b)->iter([ivar]<class... Args2>(Args2&&... args) {
        ivar->fill(std::forward<Args2>(args)...);
      });
    });

    return ivar_value(ivar);
  }

  template <class F> void iter(F&& callback)
  {
    _ivar->on_determined(std::forward<F>(callback));
  }

  lvalue_type value() & { return _ivar->value(); }
  rvalue_type value() && { return std::move(*_ivar).value(); }
  const_lvalue_type value() const& { return _ivar->value(); }

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

template <deferable_value T = void> struct Deferred {
 public:
  using value_type = T;
  using lvalue_type = std::add_lvalue_reference_t<value_type>;
  using rvalue_type = std::add_rvalue_reference_t<value_type>;

  using const_value_type = const T;
  using const_lvalue_type = std::add_lvalue_reference_t<const_value_type>;
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
    requires details::constructible_from<T, U>
  Deferred(const Deferred<U>& v)
      : Deferred(v.map([](U&& v) -> T { return std::move(v); }))
  {}

  template <class U>
    requires details::constructible_from<T, U>
  Deferred(Deferred<U>&& v)
      : Deferred(v.map([](U&& v) -> T { return std::move(v); }))
  {}

  template <class U>
    requires details::constructible_from<T, U>
  Deferred(const std::shared_ptr<Ivar<U>>& v) : Deferred(ivar_value(v))
  {}

  template <class U>
    requires details::constructible_from<T, U>
  Deferred(std::shared_ptr<Ivar<U>>&& v) : Deferred(ivar_value(v))
  {}

  template <class... Args>
    requires details::constructible_from<T, Args...>
  Deferred(Args&&... args)
      : _d(DeferredImpl<T>::create_with_value(std::forward<Args>(args)...))
  {}

  lvalue_type value() & { return _d->value(); }
  rvalue_type value() && { return std::move(*_d).value(); }
  const_lvalue_type value() const& { return _d->value(); }

  template <details::invocable<T> F> auto map(F&& callback) const
  {
    using P = details::invoke_result_t<F, T>;
    static_assert(!deferred<P>, "map function must not return a deferred");
    return _d->map(std::forward<F>(callback));
  }

  template <details::invocable<T> F> auto bind(F&& callback) const
  {
    using P = details::invoke_result_t<F, T>;
    static_assert(deferred<P>, "bind function must return a deferred");
    return _d->bind(std::forward<F>(callback));
  }

  template <details::invocable<T> F> void iter(F&& callback) const
  {
    _d->iter(std::forward<F>(callback));
  }

  bool is_determined() const { return _d->is_determined(); }

  const typename DeferredImpl<T>::ptr& impl() const { return _d; }

 private:
  typename DeferredImpl<T>::ptr _d;
};

////////////////////////////////////////////////////////////////////////////////
// Helpers
//

namespace __internal {

template <class T, class F>
struct IterVectorState
    : public std::enable_shared_from_this<IterVectorState<T, F>> {
 public:
  IterVectorState(std::vector<T>&& v, F&& f)
      : _v(std::move(v)), _f(std::move(f)), _it(_v.begin())
  {}

  Deferred<> next()
  {
    if (_it == _v.end()) { return {}; }
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

template <class T, details::invocable<T> F>
Deferred<> iter_vector(std::vector<T>&& v, F&& callback)
{
  auto state = make_shared<T, F>(std::move(v), std::move(callback));
  return state->next();
}

Deferred<bee::OrError<>> repeat(
  int times, const std::function<Deferred<bee::OrError<>>()>& f);

Deferred<> after(const bee::Span& span);

} // namespace async
