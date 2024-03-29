#include "scheduler_epoll.hpp"
#include "scheduler_test_common.hpp"

#include "bee/testing.hpp"

#ifndef __APPLE__

namespace async {

namespace {

struct SchedulerTestCommonImpl : public test::SchedulerTestCommon {
  virtual bee::OrError<SchedulerContext> create_context()
  {
    return SchedulerEpoll::create_context();
  }
} test_impl;

TEST(basic) { test_impl.basic_test(); }

TEST(large_data) { test_impl.large_data(); }

} // namespace

} // namespace async

#endif
