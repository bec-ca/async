#pragma once

#include "deferred_awaitable.hpp"
#include "ivar_multi.hpp"
#include "task.hpp"

namespace async {

template <std::copy_constructible T> struct Once {
 public:
  using Fn = std::function<Task<T>()>;

  template <class F> Once(F&& fn) : _fn(std::forward<F>(fn)) {}

  Task<T> operator()()
  {
    if (!_called) {
      schedule_task(_call, std::move(_fn), _ivar);
      _called = true;
    }
    co_return co_await _ivar->deferred_value();
  }

 private:
  using Iv = IvarMulti<T>;
  using Ivp = typename Iv::ptr;

  static Task<> _call(Fn fn, Ivp ivar)
  {
    auto result = co_await fn();
    ivar->fill(result);
    co_return;
  }

  bool _called = false;
  Ivp _ivar = Iv::create();
  Fn _fn;
};

} // namespace async
