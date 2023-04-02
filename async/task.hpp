#pragma once

#include "async.hpp"
#include "deferred_awaitable.hpp"

#include <concepts>
#include <coroutine>
#include <optional>

namespace async {

template <std::move_constructible T> struct Task;

template <std::move_constructible T> struct TaskPromise;

namespace detail {

template <class T> using handle_type = std::coroutine_handle<TaskPromise<T>>;

struct Resumable {
 public:
  using ptr = std::shared_ptr<Resumable>;

  virtual ~Resumable() {}

  virtual void resume() = 0;
};

} // namespace detail

template <class T> struct TaskState final : public detail::Resumable {
 public:
  using ptr = std::shared_ptr<TaskState>;

  TaskState(detail::handle_type<T> handle) : _handle(handle) {}

  template <std::convertible_to<T> U>
  TaskState(U&& value) : value(std::forward<U>(value))
  {
    done = true;
  }

  TaskState(const TaskState& other) = delete;
  TaskState(TaskState&& other) = delete;

  virtual ~TaskState() override { assert(done); }

  detail::Resumable::ptr await_resume;
  std::optional<T> value;
  typename Ivar<T>::ptr ivar;
  bool done = false;

  virtual void resume() override
  {
    assert(!done);
    _handle.resume();
  }

 private:
  std::coroutine_handle<> _handle;
};

template <std::move_constructible T> struct TaskPromise {
 public:
  using handle_type = detail::handle_type<T>;

  TaskPromise()
      : _task_state(make_shared<TaskState<T>>(handle_type::from_promise(*this)))
  {}

  TaskPromise(const TaskPromise& other) = delete;
  TaskPromise(TaskPromise&& other) = delete;

  ~TaskPromise() {}

  using C = Task<T>;

  C get_return_object() { return C(_task_state); }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_never final_suspend() noexcept
  {
    _task_state->done = true;
    return {};
  }

  void unhandled_exception() {}

  template <std::convertible_to<T> U> std::suspend_never return_value(U&& from)
  {
    _task_state->value.emplace(std::forward<U>(from));
    async::schedule([task_state = _task_state]() mutable {
      if (task_state->await_resume != nullptr) {
        task_state->await_resume->resume();
      } else if (task_state->ivar != nullptr) {
        task_state->ivar->resolve(std::move(*task_state->value));
      }
    });
    return {};
  }

  const typename TaskState<T>::ptr& task_state() const { return _task_state; }

 private:
  typename TaskState<T>::ptr _task_state;
};

////////////////////////////////////////////////////////////////////////////////
// is_task
//

template <std::move_constructible T> struct Task;

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

template <std::move_constructible T> struct Task {
 public:
  using value_type = T;
  using promise_type = TaskPromise<T>;
  using handle_type = typename promise_type::handle_type;

  Task(const Task& other) = default;
  Task(Task&& other) = default;

  Task& operator=(const Task& other) = default;
  Task& operator=(Task&& other) = default;

  Task(const typename TaskState<T>::ptr& task_state) : _task_state(task_state)
  {}

  template <std::convertible_to<T> U> Task(const Deferred<U>& def)
  {
    *this = [](Deferred<T> d) -> Task { co_return co_await d; }(def);
  }

  template <std::convertible_to<T> U> Task(const Task<U>& def)
  {
    *this = [](Task<U> d) -> Task { co_return co_await d; }(def);
  }

  template <std::convertible_to<T> U>
    requires(!is_deferred_t<U>::value && !is_task_v<U>)
  Task(U&& value)
      : _task_state(std::make_shared<TaskState<T>>(std::forward<U>(value)))
  {}

  ~Task() {}

  T&& value()
  {
    assert(_task_state->value.has_value());
    return std::move(*_task_state->value);
  }

  bool done() const { return _task_state->value.has_value(); }

  ////////////////////////
  // Awaitable Interface
  //

  bool await_ready() { return done(); }

  template <class U> void await_suspend(detail::handle_type<U> h)
  {
    _task_state->await_resume = h.promise().task_state();
  }

  T&& await_resume() { return std::move(value()); }

  Deferred<T> to_deferred()
  {
    if (done()) { return std::move(value()); }
    auto ivar = Ivar<T>::create();
    _task_state->ivar = ivar;
    return ivar_value(ivar);
  }

  explicit operator Deferred<T>() { return to_deferred(); }

 private:
  typename TaskState<T>::ptr _task_state;
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
inline Task<bee::Unit> schedule_task(F f, Args... args)
{
  co_return co_await f(std::move(args)...);
}

template <typename F, typename T = typename std::invoke_result_t<F>::value_type>
inline Task<std::vector<T>> repeat_parallel(int times, int concurrency, F&& f)
{
  std::vector<Task<bee::Unit>> workers;
  std::vector<T> results;
  int _counter = times;

  auto worker = [&]() -> Task<bee::Unit> {
    while (_counter > 0) {
      _counter--;
      auto result = co_await f();
      results.push_back(std::move(result));
    }
    co_return bee::unit;
  };

  for (int i = 0; i < concurrency; i++) { workers.push_back(worker()); }

  for (auto& w : workers) { co_await w; }

  co_return results;
}

} // namespace async

#define co_bail(var, or_error, msg...)                                         \
  auto __var##var = (or_error);                                                \
  if ((__var##var).is_error()) {                                               \
    (__var##var)                                                               \
      .error()                                                                 \
      .add_tag_with_location(__FILE__, __LINE__, bee::maybe_format(msg));      \
    co_return std::move((__var##var).error());                                 \
  }                                                                            \
  auto var = std::move((__var##var).value());

#define co_bail_unit(or_error, msg...)                                         \
  do {                                                                         \
    auto __var = (or_error);                                                   \
    if (__var.is_error()) {                                                    \
      __var.error().add_tag_with_location(                                     \
        __FILE__, __LINE__, bee::maybe_format(msg));                           \
      co_return std::move((__var).error());                                    \
    }                                                                          \
  } while (false)
