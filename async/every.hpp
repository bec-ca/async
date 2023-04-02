#pragma once

#include "deferred_awaitable.hpp"
#include "task.hpp"

#include "bee/span.hpp"

namespace async {

struct TaskHandle {
 public:
  using ptr = std::shared_ptr<TaskHandle>;

  TaskHandle(bee::Span span, std::function<Task<bee::Unit>()>&& fn)
      : _span(span), _fn(std::move(fn))
  {}

  Task<bee::Unit> close()
  {
    _running = false;
    _cancel_ivar->resolve(bee::unit);
    co_await (*_task);
    _fn = nullptr;
    co_return bee::unit;
  }

  void start() { _task.emplace(_start()); }

 private:
  Task<bee::Unit> _start()
  {
    while (_running) {
      co_await _fn();
      auto continue_ivar = Ivar<bee::Unit>::create();
      _cancel_ivar = Ivar<bee::Unit>::create();
      _cancel_ivar->on_resolved([continue_ivar](bee::Unit) {
        if (!continue_ivar->is_resolved()) {
          continue_ivar->resolve(bee::unit);
        }
      });
      auto task_id = async::after(_span, [continue_ivar]() {
        if (!continue_ivar->is_resolved()) {
          continue_ivar->resolve(bee::unit);
        }
      });
      co_await continue_ivar;
      if (!_running) { cancel(task_id); }
    }
    co_return bee::unit;
  }

  bool _running = true;
  bee::Span _span;
  std::function<Task<bee::Unit>()> _fn;

  Ivar<bee::Unit>::ptr _cancel_ivar;
  std::optional<Task<bee::Unit>> _task;
};

template <class F> typename TaskHandle::ptr every(bee::Span span, F&& f)
{
  auto handle = std::make_shared<TaskHandle>(span, std::forward<F>(f));
  handle->start();
  return handle;
}

} // namespace async
