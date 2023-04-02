#include "async_command.hpp"

#include "scheduler_selector.hpp"
#include "task.hpp"

using std::function;
using std::optional;

namespace async {

command::Cmd run_async(
  command::CommandBuilder& builder,
  const function<Deferred<bee::OrError<bee::Unit>>()>& f)
{
  return builder.run([=]() -> bee::OrError<bee::Unit> {
    bail(scheduler_context, SchedulerSelector::create_context());
    auto& scheduler = scheduler_context.scheduler();
    optional<bee::OrError<bee::Unit>> resolved_value;
    f().iter([&](bee::OrError<bee::Unit>&& v) {
      resolved_value.emplace(std::move(v));
    });
    bail_unit(scheduler.wait_until(
      [&resolved_value]() { return resolved_value.has_value(); }));
    return std::move(resolved_value.value());
  });
}

command::Cmd run_coro(
  command::CommandBuilder& builder, function<Task<bee::OrError<bee::Unit>>()> f)
{
  return builder.run([=]() -> bee::OrError<bee::Unit> {
    bail(scheduler_context, SchedulerSelector::create_context());
    auto& scheduler = scheduler_context.scheduler();
    auto t = f();
    bail_unit(scheduler.wait_until([&t]() { return t.done(); }));
    return std::move(t.value());
  });
}

} // namespace async
