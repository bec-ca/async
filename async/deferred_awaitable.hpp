#pragma once

#include "async.hpp"

#include "bee/copy.hpp"

#include <coroutine>
#include <memory>

namespace async {

template <std::move_constructible T> struct DeferredAwaitable {
 public:
  DeferredAwaitable(Deferred<T>&& def)
      : _deferred(std::move(def)),
        _value(std::make_shared<std::optional<T>>(std::nullopt))
  {}

  DeferredAwaitable(const Deferred<T>& def) : DeferredAwaitable(bee::copy(def))
  {}

  ~DeferredAwaitable() {}

  bool await_ready() { return _value->has_value(); }

  template <class U> void await_suspend(std::coroutine_handle<U> h)
  {
    _deferred.iter([value = _value, h](T&& incoming_value) {
      value->emplace(std::move(incoming_value));
      h.resume();
    });
  }

  T&& await_resume()
  {
    assert(_value->has_value());
    return std::move(_value->value());
  }

 private:
  Deferred<T> _deferred;

  std::shared_ptr<std::optional<T>> _value;
};

template <std::move_constructible T>
DeferredAwaitable<T> operator co_await(Deferred<T>&& def)
{
  return DeferredAwaitable<T>(std::move(def));
}

template <std::move_constructible T>
DeferredAwaitable<T> operator co_await(const Deferred<T>& def)
{
  return DeferredAwaitable<T>(def);
}

template <std::move_constructible T>
DeferredAwaitable<T> operator co_await(std::shared_ptr<Ivar<T>>&& def)
{
  return DeferredAwaitable<T>((std::move(def)));
}

template <std::move_constructible T>
DeferredAwaitable<T> operator co_await(const std::shared_ptr<Ivar<T>>& def)
{
  return DeferredAwaitable<T>(def);
}

} // namespace async
