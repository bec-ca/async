#pragma once

#include <cassert>
#include <concepts>
#include <memory>
#include <optional>
#include <queue>

#include "async.hpp"
#include "deferred_awaitable.hpp"
#include "task.hpp"

#include "bee/format.hpp"

namespace async {

template <class T> struct Pipe : public std::enable_shared_from_this<Pipe<T>> {
 public:
  using ptr = std::shared_ptr<Pipe<T>>;

  Pipe(const Pipe& other) = delete;
  Pipe(Pipe&& other) = delete;

  ~Pipe() { close(); }

  static ptr create() { return ptr(new Pipe<T>()); }

  template <std::convertible_to<T> U> void push(U&& v)
  {
    assert(!_closed);

    if (_waiting_pop.empty()) {
      _queue.push(std::forward<U>(v));
    } else {
      _waiting_pop.front()->fill(std::forward<U>(v));
      _waiting_pop.pop();
    }
  }

  template <std::convertible_to<T> U> async::Task<> blocking_push(U&& v)
  {
    assert(!_closed);
    while (true) {
      if (!_waiting_pop.empty()) {
        _waiting_pop.front()->fill(std::forward<U>(v));
        _waiting_pop.pop();
        co_return;
      }
      auto ivar = Ivar<>::create();
      _waiting_push.push(ivar);
      co_await ivar;
    }
  }

  template <class... Args> void emplace(Args&&... args)
  {
    push(T(std::forward<Args>(args)...));
  }

  Task<std::optional<T>> next_value()
  {
    if (!_queue.empty()) {
      auto ret = std::move(_queue.front());
      _queue.pop();
      co_return std::move(ret);
    } else if (_closed) {
      co_return std::nullopt;
    } else {
      auto ivar = Ivar<std::optional<T>>::create();
      _waiting_pop.push(ivar);

      if (!_waiting_push.empty()) {
        _waiting_push.front()->fill();
        _waiting_push.pop();
      }

      co_return co_await ivar;
    }
  }

  template <std::invocable<T> F> Task<> iter(F&& f)
  {
    return _iter(this->shared_from_this(), std::forward<F>(f));
  }

  template <std::invocable<T> F> Task<> iter2(F&& f)
  {
    return _iter2(this->shared_from_this(), std::forward<F>(f));
  }

  template <typename R, std::invocable<T> F> typename Pipe<R>::ptr map(F&& f)
  {
    auto out_pipe = Pipe<R>::create();
    async::schedule_task(
      [ptr = this->shared_from_this(), out_pipe, f]() -> Task<> {
        while (true) {
          auto value = co_await ptr->next_value();
          if (!value.has_value() || out_pipe->is_closed()) { break; }
          out_pipe->push(f(std::move(*value)));
        }
        out_pipe->close();
      });
    return out_pipe;
  }

  void close()
  {
    if (_closed) return;
    _closed = true;
    while (!_waiting_pop.empty()) {
      _waiting_pop.front()->fill(std::nullopt);
      _waiting_pop.pop();
    }
    while (!_waiting_push.empty()) {
      _waiting_push.front()->fill();
      _waiting_push.pop();
    }
  }

  bool is_closed() const { return _closed; }

 private:
  Pipe() {}

  template <std::invocable<T> F>
  static Task<> _iter(std::weak_ptr<Pipe> weak_ptr, F f)
  {
    while (auto ptr = weak_ptr.lock()) {
      auto next = std::move(co_await ptr->next_value());
      if (!next.has_value()) { break; }
      f(std::move(*next));
    }
  }

  template <std::invocable<T> F>
  static Task<> _iter2(std::weak_ptr<Pipe> weak_ptr, F f)
  {
    while (auto ptr = weak_ptr.lock()) {
      auto next = co_await ptr->next_value();
      if (!next.has_value()) { break; }
      co_await f(std::move(*next));
    }
  }

  std::queue<T> _queue;
  std::queue<typename Ivar<std::optional<T>>::ptr> _waiting_pop;
  std::queue<typename Ivar<>::ptr> _waiting_push;

  bool _closed = false;
};

} // namespace async
