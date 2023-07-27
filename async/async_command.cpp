#include "async_command.hpp"

#include "run_scheduler.hpp"
#include "task.hpp"

using std::function;

namespace async {

command::Cmd command(
  command::CommandBuilder& builder, function<Task<bee::OrError<>>()> f)
{
  return builder.run([=]() -> bee::OrError<> {
    bee::OrError<> result;
    RunScheduler::run([&]() -> Task<> { result = co_await f(); });
    bail_unit(result);
    return bee::ok();
  });
}

} // namespace async
