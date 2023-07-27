#include "testing.hpp"

#include "run_scheduler.hpp"

using std::function;

namespace async {

void run_async_test(function<Task<>()>&& test)
{
  RunScheduler::run(std::move(test));
}

} // namespace async
