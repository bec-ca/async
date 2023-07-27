#pragma once

#include <coroutine>
#include <memory>
#include <type_traits>

#include "async.hpp"

#include "bee/copy.hpp"

namespace async {

template <deferable_value T> struct DeferredAwaitable {
 public:
  using value_type = T;
  using rvalue_type = std::add_rvalue_reference_t<value_type>;

  DeferredAwaitable(Deferred<T>&& def) : _deferred(std::move(def)) {}

  DeferredAwaitable(const Deferred<T>& def) : DeferredAwaitable(bee::copy(def))
  {}

  ~DeferredAwaitable() {}

  bool await_ready() { return _deferred.is_determined(); }

  void await_suspend(std::coroutine_handle<> h)
  {
    _deferred.iter([h](auto&&...) { h.resume(); });
  }

  rvalue_type await_resume() { return std::move(_deferred).value(); }

 private:
  Deferred<T> _deferred;
};

template <deferable_value T>
DeferredAwaitable<T> operator co_await(Deferred<T>&& def)
{
  return DeferredAwaitable<T>(std::move(def));
}

template <deferable_value T>
DeferredAwaitable<T> operator co_await(const Deferred<T>& def)
{
  return DeferredAwaitable<T>(def);
}

template <deferable_value T>
DeferredAwaitable<T> operator co_await(std::shared_ptr<Ivar<T>>&& def)
{
  return DeferredAwaitable<T>((std::move(def)));
}

template <deferable_value T>
DeferredAwaitable<T> operator co_await(const std::shared_ptr<Ivar<T>>& def)
{
  return DeferredAwaitable<T>(def);
}

} // namespace async
