#include "async.hpp"
#include "deferred_awaitable.hpp"
#include "scheduler_selector.hpp"
#include "task.hpp"
#include "testing.hpp"

#include "bee/format.hpp"
#include "bee/testing.hpp"

using std::string;

namespace async {
namespace {

ASYNC_TEST(basic)
{
  auto ivar = Ivar<bee::OrError<string>>::create();
  auto def_str = ivar_value(ivar);

  auto done = def_str.map([](const bee::OrError<string>& str) {
    P(str);
    return bee::ok();
  });

  ivar->fill("yo!");

  co_await done;
}

ASYNC_TEST(basic_void)
{
  auto ivar = Ivar<>::create();
  ivar->fill();
  co_await ivar_value(ivar);
  P("done");
}

ASYNC_TEST(basic_void_with_value)
{
  auto ivar = Ivar<>::create_with_value();
  co_await ivar_value(ivar);
  P("done");
}

ASYNC_TEST(map)
{
  auto ivar1 = Ivar<string>::create();
  auto def1 = ivar_value(ivar1);

  auto def2 =
    def1.map([](const string& str) -> string { return F("got '$'", str); });

  auto done = def2.map([](const string& str) {
    P("resolved value: $", str);
    return bee::ok();
  });

  ivar1->fill("hello");

  co_await done;
}

TEST(bind)
{
  must(ctx, SchedulerSelector::create_context());
  auto& scheduler = ctx.scheduler();

  auto ivar1 = Ivar<string>::create();
  auto def1 = ivar_value(ivar1);

  auto ivar_inner = Ivar<string>::create();
  Deferred<string> def_inner = ivar_value(ivar_inner);

  auto def2 = def1.bind([def_inner](const string& str) -> Deferred<string> {
    return def_inner.map([str](const string& inner_str) -> string {
      return F("outer:'$' inner:'$'", str, inner_str);
    });
  });

  def2.iter([](const string& str) { P("resolved value: $", str); });

  scheduler.wait_until([]() { return true; });
  P("Before resolving anything");

  ivar1->fill("hello");
  scheduler.wait_until([]() { return true; });

  P("Before resolving def1");

  ivar_inner->fill("inner string");
  scheduler.wait_until([]() { return true; });

  P("After resolving def_inner");

  scheduler.wait_until([]() { return true; });
}

TEST(bind2)
{
  must(ctx, SchedulerSelector::create_context());
  auto& scheduler = ctx.scheduler();

  auto ivar1 = Ivar<string>::create();
  auto def1 = ivar_value(ivar1);

  auto ivar_inner = Ivar<string>::create();
  auto def_inner = ivar_value(ivar_inner);

  auto def2 = def1.bind([def_inner](const string& str) -> Deferred<string> {
    return def_inner.map(
      [str = std::move(str)](const string& inner_str) -> string {
        return F("outer:'$' inner:'$'", str, inner_str);
      });
  });

  def2.iter([](const string& str) { P("resolved value: $", str); });

  scheduler.wait_until([]() { return true; });
  P("Before resolving anything");

  ivar_inner->fill("inner string");
  scheduler.wait_until([]() { return true; });

  P("Before resolving def_inner");

  ivar1->fill("hello");
  scheduler.wait_until([]() { return true; });

  P("After resolving def1");

  scheduler.wait_until([]() { return true; });
}

ASYNC_TEST(bind3)
{
  auto ivar1 = Ivar<string>::create();
  auto def1 = ivar_value(ivar1);

  auto ivar_inner = Ivar<string>::create();
  auto def_inner = ivar_value(ivar_inner);

  auto done =
    def1
      .bind([def_inner](const string& str) -> Deferred<string> {
        return def_inner.map([str = std::move(str)](const string& inner_str) {
          return F("outer:'$' inner:'$'", str, inner_str);
        });
      })
      .map([](const string& str) {
        P("resolved value: $", str);
        return bee::unit;
      });

  ivar_inner->fill("inner string");
  ivar1->fill("hello");

  co_await done;
}

ASYNC_TEST(compile_error)
{
  auto ivar = Ivar<>::create();
  auto out = ivar_value(ivar).bind([]() -> Deferred<> { return {}; });

  ivar->fill();
  co_await out;
}

} // namespace
} // namespace async
