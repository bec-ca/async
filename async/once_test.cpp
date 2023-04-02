#include "once.hpp"

#include "testing.hpp"

using bee::print_line;

namespace async {
namespace {

Task<bee::Unit> other() { co_return bee::unit; }

Task<bee::Unit> fn()
{
  print_line("Fn called");
  co_await other();
  co_return bee::unit;
}

CORO_TEST(should_call_fn_only_once)
{
  Once<bee::Unit> once(fn);

  auto d1 = once();
  auto d2 = once();
  auto d3 = once();

  co_await d1;
  co_await d2;
  co_await d3;

  co_return bee::ok();
}

CORO_TEST(call_and_exit)
{
  Once<bee::Unit> once(fn);
  once();
  co_return bee::ok();
}

} // namespace
} // namespace async
