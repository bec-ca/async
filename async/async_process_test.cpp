#include <optional>

#include "async_process.hpp"
#include "deferred_awaitable.hpp"
#include "scheduler_selector.hpp"
#include "testing.hpp"

#include "bee/format_optional.hpp"

namespace async {
namespace {

ASYNC_TEST(basic)
{
  auto exit_status_ivar = Ivar<int>::create();
  AsyncProcess::spawn_process(
    {.cmd = bee::FilePath::of_string("echo"), .args = {"hello world"}},
    [exit_status_ivar](int status) { exit_status_ivar->fill(status); });

  auto exit_status = co_await exit_status_ivar;
  P("Process exited with status $", exit_status);
}

} // namespace
} // namespace async
