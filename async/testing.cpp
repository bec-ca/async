#include "testing.hpp"

#include "run_scheduler.hpp"

using std::function;

namespace async {

void run_coro_test(function<Task<bee::OrError<bee::Unit>>()>&& test)
{
  must_unit(RunScheduler::run_coro(std::move(test)));
}

void run_async_test(function<Deferred<bee::OrError<bee::Unit>>()>&& test)
{
  must_unit(RunScheduler::run_async(std::move(test)));
}

} // namespace async
