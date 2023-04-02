#include "task.hpp"

#include "deferred_awaitable.hpp"
#include "testing.hpp"

using bee::print_line;
using bee::Span;
using std::string;

namespace async {
namespace {

Deferred<string> get_other_string()
{
  print_line("get_other_string");
  return "this is the other string";
}

Task<string> get_string()
{
  print_line("get_string");
  co_return "this is the string";
}

Task<string> get_slow_string()
{
  co_await after(Span::of_seconds(0.1));
  print_line("get_slow_string");
  co_return "this is the the *slooooow* string";
}

Task<bee::OrError<bee::Unit>> simple_task()
{
  print_line("simple_task");
  auto str = co_await get_string();
  auto str2 = co_await get_other_string();
  auto str3 = co_await get_slow_string();
  print_line("$ -- $", str, str2, str3);
  co_return bee::unit;
}

CORO_TEST(coro_basic) { co_return co_await simple_task(); }

} // namespace
} // namespace async
