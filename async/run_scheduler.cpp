#include "run_scheduler.hpp"

#include "async/scheduler_selector.hpp"

using std::function;

namespace async {

void RunScheduler::run(function<Task<>()>&& fn)
{
  must(poll_context, SchedulerSelector::create_context());
  run(std::move(fn), std::move(poll_context));
}

void RunScheduler::run(function<Task<>()>&& fn, SchedulerContext&& ctx)
{
  auto t = fn();
  ctx.scheduler().wait_until([&]() { return t.done(); });
}

} // namespace async
