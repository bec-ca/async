#pragma once

#include "deferred_awaitable.hpp"
#include "task.hpp"

#include "bee/span.hpp"

namespace async {

struct TaskHandle {
 public:
  using ptr = std::shared_ptr<TaskHandle>;

  TaskHandle(bee::Span span, std::function<Task<>()>&& fn)
      : _span(span), _fn(std::move(fn))
  {}

  Task<> close()
  {
    _running = false;
    _cancel_ivar->fill();
    co_await (*_task);
    _fn = nullptr;
  }

  void start() { _task.emplace(_start()); }

 private:
  Task<> _start()
  {
    while (_running) {
      co_await _fn();
      auto continue_ivar = Ivar<>::create();
      _cancel_ivar = Ivar<>::create();
      _cancel_ivar->on_determined([continue_ivar]() {
        if (!continue_ivar->is_determined()) { continue_ivar->fill(); }
      });
      auto task_id = async::after(_span, [continue_ivar]() {
        if (!continue_ivar->is_determined()) { continue_ivar->fill(); }
      });
      co_await continue_ivar;
      if (!_running) { cancel(task_id); }
    }
  }

  bool _running = true;
  bee::Span _span;
  std::function<Task<>()> _fn;

  Ivar<>::ptr _cancel_ivar;
  std::optional<Task<>> _task;
};

template <class F> typename TaskHandle::ptr every(bee::Span span, F&& f)
{
  auto handle = std::make_shared<TaskHandle>(span, std::forward<F>(f));
  handle->start();
  return handle;
}

} // namespace async
