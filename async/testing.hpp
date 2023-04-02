#pragma once

#include "async.hpp"
#include "task.hpp"

#include "bee/testing.hpp"

#include <functional>

#define ASYNC_TEST(name)                                                       \
  async::Deferred<bee::OrError<bee::Unit>> async_test_##name();                \
  TEST(name)                                                                   \
  {                                                                            \
    run_async_test(async_test_##name);                                         \
  }                                                                            \
  async::Deferred<bee::OrError<bee::Unit>> async_test_##name()

#define CORO_TEST(name)                                                        \
  async::Task<bee::OrError<bee::Unit>> coro_test_##name();                     \
  TEST(name)                                                                   \
  {                                                                            \
    run_coro_test(coro_test_##name);                                           \
  }                                                                            \
  async::Task<bee::OrError<bee::Unit>> coro_test_##name()

namespace async {

void run_async_test(
  std::function<async::Deferred<bee::OrError<bee::Unit>>()>&& test);

void run_coro_test(
  std::function<async::Task<bee::OrError<bee::Unit>>()>&& test);

} // namespace async
