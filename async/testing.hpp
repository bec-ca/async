#pragma once

#include <functional>

#include "async.hpp"
#include "task.hpp"

#include "bee/testing.hpp"

#define ASYNC_TEST(name)                                                       \
  async::Task<> async_test_##name();                                           \
  TEST(name) { run_async_test(async_test_##name); }                            \
  async::Task<> async_test_##name()

namespace async {

void run_async_test(std::function<async::Task<>()>&& test);

} // namespace async
