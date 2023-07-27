#include "once.hpp"
#include "testing.hpp"

namespace async {
namespace {

Task<> other() { co_return; }

Task<std::string> fn()
{
  P("Fn called");
  co_await other();
  co_return "";
}

ASYNC_TEST(should_call_fn_only_once)
{
  Once<std::string> once(fn);

  auto d1 = once();
  auto d2 = once();
  auto d3 = once();

  co_await d1;
  co_await d2;
  co_await d3;
}

ASYNC_TEST(call_and_exit)
{
  Once<std::string> once(fn);
  co_await once();
}

} // namespace
} // namespace async
