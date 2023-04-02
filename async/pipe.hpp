#pragma once

#include "async.hpp"
#include "bee/format.hpp"
#include "deferred_awaitable.hpp"
#include "task.hpp"

#include <cassert>
#include <concepts>
#include <memory>
#include <optional>

namespace async {

template <class T> struct Pipe : public std::enable_shared_from_this<Pipe<T>> {
 public:
  using ptr = std::shared_ptr<Pipe<T>>;

  Pipe(const Pipe& other) = delete;
  Pipe(Pipe&& other) = delete;

  ~Pipe() { close(); }

  static ptr create() { return ptr(new Pipe<T>()); }

  void push(T&& v)
  {
    assert(!_closed);

    if (_waiting.empty()) {
      _queue.push(std::move(v));
    } else {
      _waiting.front()->resolve(std::move(v));
      _waiting.pop();
    }
  }

  void push(const T& v) { push(T(v)); }

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
      _waiting.push(ivar);
      co_return co_await ivar;
    }
  }

  template <std::invocable<T> F> Task<bee::Unit> iter_values(F&& f)
  {
    return _iter_values(this->shared_from_this(), std::forward<F>(f));
  }

  template <std::invocable<T> F> Task<bee::Unit> iter_values2(F&& f)
  {
    return _iter_values2(this->shared_from_this(), std::forward<F>(f));
  }

  template <typename R, std::invocable<T> F> typename Pipe<R>::ptr map(F&& f)
  {
    auto out_pipe = Pipe<R>::create();
    async::schedule_task(
      [ptr = this->shared_from_this(), out_pipe, f]() -> Task<bee::Unit> {
        while (true) {
          auto value = co_await ptr->next_value();
          if (!value.has_value() || out_pipe->is_closed()) { break; }
          out_pipe->push(f(std::move(*value)));
        }
        out_pipe->close();
        co_return bee::unit;
      });
    return out_pipe;
  }

  void close()
  {
    if (_closed) return;
    _closed = true;
    while (!_waiting.empty()) {
      _waiting.front()->resolve(std::nullopt);
      _waiting.pop();
    }
  }

  bool is_closed() const { return _closed; }

 private:
  Pipe() {}

  template <std::invocable<T> F>
  static Task<bee::Unit> _iter_values(std::weak_ptr<Pipe> weak_ptr, F f)
  {
    while (auto ptr = weak_ptr.lock()) {
      auto next = std::move(co_await ptr->next_value());
      if (!next.has_value()) { break; }
      f(std::move(*next));
    }
    co_return bee::unit;
  }

  template <std::invocable<T> F>
  static Task<bee::Unit> _iter_values2(std::weak_ptr<Pipe> weak_ptr, F f)
  {
    while (auto ptr = weak_ptr.lock()) {
      auto next = co_await ptr->next_value();
      if (!next.has_value()) { break; }
      co_await f(std::move(*next));
    }
    co_return bee::unit;
  }

  std::queue<T> _queue;
  std::queue<typename Ivar<std::optional<T>>::ptr> _waiting;

  bool _closed = false;
};

} // namespace async
