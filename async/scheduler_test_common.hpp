#include "scheduler_context.hpp"

namespace async {

namespace test {

struct SchedulerTestCommon {
 public:
  void basic_test();
  void large_data();

  virtual bee::OrError<SchedulerContext> create_context() = 0;
};

} // namespace test

} // namespace async
