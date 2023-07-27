#include "deferred_awaitable.hpp"
#include "task.hpp"
#include "testing.hpp"

using bee::Span;
using std::string;

namespace async {
namespace {

Deferred<string> get_other_string()
{
  P("get_other_string");
  return "this is the other string";
}

Task<string> get_string()
{
  P("get_string");
  co_return "this is the string";
}

Task<string> get_slow_string()
{
  co_await after(Span::of_seconds(0.1));
  P("get_slow_string");
  co_return "this is the the *slooooow* string";
}

Task<bee::OrError<>> simple_task()
{
  P("simple_task");
  auto str = co_await get_string();
  auto str2 = co_await get_other_string();
  auto str3 = co_await get_slow_string();
  P("$ -- $ -- $", str, str2, str3);
  co_return bee::ok();
}

ASYNC_TEST(task_basic) { co_await simple_task(); }

ASYNC_TEST(void_task)
{
  auto f = []() -> Task<> { co_return; };
  auto g = [&]() -> Task<> { co_await f(); };

  co_await g();
  P("done");
}

} // namespace
} // namespace async
