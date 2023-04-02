#include "async_process.hpp"

#include "bee/format_optional.hpp"
#include "deferred_awaitable.hpp"
#include "scheduler_selector.hpp"
#include "testing.hpp"

#include <optional>

namespace async {

namespace {

CORO_TEST(basic)
{
  auto exit_status_ivar = Ivar<int>::create();
  AsyncProcess::spawn_process(
    {.cmd = "echo", .args = {"hello world"}},
    [exit_status_ivar](int status) { exit_status_ivar->resolve(status); });

  auto exit_status = co_await exit_status_ivar;
  bee::print_line("Process exited with status $", exit_status);

  co_return bee::unit;
}

} // namespace

} // namespace async
