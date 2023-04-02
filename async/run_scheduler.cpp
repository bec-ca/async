#include "run_scheduler.hpp"

#include "async/deferred_awaitable.hpp"
#include "async/scheduler_selector.hpp"

using std::function;

namespace async {

bee::OrError<bee::Unit> RunScheduler::run_coro(
  function<Task<bee::OrError<bee::Unit>>()>&& fn)
{
  bail(poll_context, SchedulerSelector::create_context());
  return run_coro(std::move(fn), std::move(poll_context));
}

bee::OrError<bee::Unit> RunScheduler::run_coro(
  function<Task<bee::OrError<bee::Unit>>()>&& fn, SchedulerContext&& ctx)
{
  auto t = fn();
  ctx.scheduler().wait_until([&]() { return t.done(); });
  return t.value();
}

bee::OrError<bee::Unit> RunScheduler::run_async(
  function<Deferred<bee::OrError<bee::Unit>>()>&& fn)
{
  return run_coro([fn = std::move(fn)]() -> Task<bee::OrError<bee::Unit>> {
    co_return co_await fn();
  });
}

} // namespace async
