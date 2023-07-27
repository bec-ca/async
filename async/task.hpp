#pragma once

#include <concepts>
#include <coroutine>
#include <optional>
#include <type_traits>

#include "async.hpp"
#include "deferred_awaitable.hpp"

#include "bee/unit.hpp"

namespace async {

template <deferable_value T = void> struct Task;

template <deferable_value T> struct TaskPromise;

namespace detail {

template <deferable_value T>
using handle_type = std::coroutine_handle<TaskPromise<T>>;

struct Resumable {
 public:
  using ptr = std::shared_ptr<Resumable>;

  virtual ~Resumable() {}

  virtual void resume() = 0;
};

} // namespace detail

template <deferable_value T> struct TaskState final : public detail::Resumable {
 public:
  using ptr = std::shared_ptr<TaskState>;

  using value_type = T;
  using const_value_type = const T;
  using rvalue_type = std::add_rvalue_reference_t<value_type>;
  using lvalue_type = std::add_rvalue_reference_t<value_type>;
  using const_lvalue_type = std::add_rvalue_reference_t<const_value_type>;

  TaskState(std::coroutine_handle<> handle) : _handle(handle) {}

  template <std::convertible_to<T> U>
  TaskState(U&& value) : _value(std::forward<U>(value))
  {
    done = true;
  }

  TaskState(const TaskState& other) = delete;
  TaskState(TaskState&& other) = delete;

  virtual ~TaskState() override { assert(done); }

  detail::Resumable::ptr await_resume;

  typename Ivar<T>::ptr ivar;
  bool done = false;

  virtual void resume() override
  {
    assert(!done);
    _handle.resume();
  }

  template <class... Args>
    requires details::constructible_from<T, Args...>
  void emplace_value(Args&&... args)
  {
    _value.emplace(std::forward<Args>(args)...);
  }

  rvalue_type value() &&
  {
    if constexpr (!std::is_void_v<T>) { return std::move(_value).value(); }
  }

  lvalue_type value() &
  {
    if constexpr (!std::is_void_v<T>) { return _value.value(); }
  }

  const_lvalue_type value() const&
  {
    if constexpr (!std::is_void_v<T>) { return _value.value(); }
  }

  bool has_value() const { return _value.has_value(); }

 private:
  std::optional<bee::unit_if_void_t<T>> _value;

  std::coroutine_handle<> _handle;
};

template <class P, class T> struct TaskPromiseBase {
 public:
  using handle_type = std::coroutine_handle<P>;
  using state_t = TaskState<T>;

  TaskPromiseBase()
      : _task_state(make_shared<state_t>(handle_type::from_promise(parent())))
  {}

  TaskPromiseBase(const TaskPromiseBase& other) = delete;
  TaskPromiseBase(TaskPromiseBase&& other) = delete;

  ~TaskPromiseBase() {}

  using C = Task<T>;

  C get_return_object() { return C(_task_state); }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_never final_suspend() noexcept
  {
    _task_state->done = true;
    return {};
  }

  void unhandled_exception()
  {
    // TODO: This exception to be propagated to the right spot
    throw;
  }

  const typename state_t::ptr& task_state() const { return _task_state; }

 protected:
  template <class... Args>
    requires details::constructible_from<T, Args...>
  std::suspend_never _return_value(Args&&... args)
  {
    _task_state->emplace_value(std::forward<Args>(args)...);
    if (_task_state->await_resume != nullptr) {
      async::schedule([task_state = _task_state]() mutable {
        task_state->await_resume->resume();
      });
    } else if (_task_state->ivar != nullptr) {
      if constexpr (std::is_void_v<T>) {
        _task_state->ivar->fill();
      } else {
        _task_state->ivar->fill(std::move(*_task_state).value());
      }
    }
    return {};
  }

 private:
  typename state_t::ptr _task_state;

  P& parent() { return static_cast<P&>(*this); }
  const P& parent() const { return static_cast<const P&>(*this); }
};

template <>
struct TaskPromise<void> : public TaskPromiseBase<TaskPromise<void>, void> {
 private:
  using parent = TaskPromiseBase<TaskPromise<void>, void>;

 public:
  auto return_void() { return parent::_return_value(); }
};

template <deferable_value T>
struct TaskPromise : public TaskPromiseBase<TaskPromise<T>, T> {
 private:
  using parent = TaskPromiseBase<TaskPromise<T>, T>;

 public:
  template <std::convertible_to<T> U> std::suspend_never return_value(U&& from)
  {
    return parent::_return_value(std::forward<U>(from));
  }
};

////////////////////////////////////////////////////////////////////////////////
// is_task
//

template <class T> struct is_task_t;

template <class T> struct is_task_t<Task<T>> {
  static constexpr bool value = true;
};

template <class T> struct is_task_t {
  static constexpr bool value = false;
};

template <class T>
concept task = is_task_t<T>::value;

template <class T>
concept task_function = is_task_t<T>::value;

template <class T> constexpr bool is_task_v = is_task_t<T>::value;

////////////////////////////////////////////////////////////////////////////////
// Task
//

template <deferable_value T> struct Task {
 public:
  using value_type = T;

  using rvalue_type = std::add_rvalue_reference_t<value_type>;

  using promise_type = TaskPromise<T>;
  using handle_type = typename promise_type::handle_type;
  using state_t = typename promise_type::state_t;

  Task(const Task& other) = default;
  Task(Task&& other) = default;

  Task& operator=(const Task& other) = default;
  Task& operator=(Task&& other) = default;

  Task(const typename state_t::ptr& task_state) : _task_state(task_state) {}

  template <std::convertible_to<T> U> Task(const Deferred<U>& def)
  {
    *this = [](Deferred<U> d) -> Task { co_return co_await d; }(def);
  }

  template <std::convertible_to<T> U> Task(const Task<U>& def)
  {
    *this = [](Task<U> d) -> Task { co_return co_await d; }(def);
  }

  template <std::convertible_to<value_type> U>
    requires(!is_deferred_v<U> && !is_task_v<U>)
  Task(U&& value)
      : _task_state(
          std::make_shared<TaskState<value_type>>(std::forward<U>(value)))
  {}

  ~Task() {}

  rvalue_type value()
  {
    assert(_task_state->has_value());
    return std::move(*_task_state).value();
  }

  bool done() const { return _task_state->has_value(); }

  ////////////////////////////////////////////////////////////////////////////////
  // Awaitable Interface
  //

  bool await_ready() { return done(); }

  template <class U> void await_suspend(detail::handle_type<U> h)
  {
    _task_state->await_resume = h.promise().task_state();
  }

  rvalue_type await_resume() { return value(); }

  ////////////////////////////////////////////////////////////////////////////////
  // Deferred compatibility
  //

  Deferred<value_type> to_deferred()
  {
    if (done()) {
      if constexpr (std::is_void_v<T>) {
        return {};
      } else {
        return value();
      }
    }
    auto ivar = Ivar<value_type>::create();
    _task_state->ivar = ivar;
    return ivar_value(ivar);
  }

  explicit operator Deferred<value_type>() { return to_deferred(); }

 private:
  typename state_t::ptr _task_state;
};

////////////////////////////////////////////////////////////////////////////////
// Never
//

struct NeverAwaitable {
 public:
  bool await_ready() { return false; }

  template <class U> void await_suspend(std::coroutine_handle<U>) {}

  void await_resume() {}
};

inline NeverAwaitable operator co_await(const never_t&)
{
  return NeverAwaitable();
}

////////////////////////////////////////////////////////////////////////////////
// utils
//

template <class F, class... Args>
  requires std::invocable<F, Args...> && task<std::invoke_result_t<F, Args...>>
inline Task<> schedule_task(F f, Args... args)
{
  co_await f(std::move(args)...);
}

template <class F, class T = typename std::invoke_result_t<F>::value_type>
inline Task<std::vector<T>> repeat_parallel(int times, int concurrency, F&& f)
{
  std::vector<Task<>> workers;
  std::vector<T> results;
  int _counter = times;

  auto worker = [&]() -> Task<> {
    while (_counter > 0) {
      _counter--;
      auto result = co_await f();
      results.push_back(std::move(result));
    }
  };

  for (int i = 0; i < concurrency; i++) { workers.push_back(worker()); }

  for (auto& w : workers) { co_await w; }

  co_return results;
}

template <class T, class F>
inline Task<> iter_parallel(T&& container, int concurrency, F&& f)
{
  std::vector<Task<>> workers;
  auto it = container.begin();
  auto end = container.end();
  auto worker = [&]() -> Task<> {
    while (it != end) {
      auto& v = *(it++);
      co_await f(v);
    }
  };
  for (int i = 0; i < concurrency; i++) { workers.push_back(worker()); }
  for (auto& w : workers) { co_await w; }
}

} // namespace async

#define co_bail(var, or_error, msg...)                                         \
  auto __var##var = (or_error);                                                \
  if ((__var##var).is_error()) {                                               \
    (__var##var).error().add_tag_with_location(HERE, bee::maybe_format(msg));  \
    co_return std::move((__var##var).error());                                 \
  }                                                                            \
  auto var = std::move(__var##var).value();

#define co_bail_unit(or_error, msg...)                                         \
  do {                                                                         \
    auto __var = (or_error);                                                   \
    if (__var.is_error()) {                                                    \
      __var.error().add_tag_with_location(HERE, bee::maybe_format(msg));       \
      co_return std::move(__var).error();                                      \
    }                                                                          \
  } while (false)
