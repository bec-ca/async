#include "bee/testing.hpp"

#include "scheduler_poll.hpp"
#include "scheduler_test_common.hpp"

namespace async {
namespace {

struct SchedulerTestCommonImpl : public test::SchedulerTestCommon {
  virtual bee::OrError<SchedulerContext> create_context()
  {
    return SchedulerPoll::create_context();
  }
} test_impl;

TEST(basic) { test_impl.basic_test(); }

TEST(large_data) { test_impl.large_data(); }

} // namespace
} // namespace async
