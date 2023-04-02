#include "async.hpp"

#include "scheduler_selector.hpp"

#include "bee/format.hpp"
#include "bee/testing.hpp"
#include "deferred_awaitable.hpp"
#include "task.hpp"
#include "testing.hpp"

using bee::format;
using bee::print_line;
using std::string;

namespace async {
namespace {

CORO_TEST(basic)
{
  auto ivar = Ivar<bee::OrError<string>>::create();
  auto def_str = ivar_value(ivar);

  auto done = def_str.map([](const bee::OrError<string>& str) {
    print_line(str);
    return bee::ok();
  });

  ivar->resolve("yo!");

  co_return co_await done;
}

CORO_TEST(map)
{
  auto ivar1 = Ivar<string>::create();
  auto def1 = ivar_value(ivar1);

  auto def2 = def1.map(
    [](const string& str) -> string { return format("got '$'", str); });

  auto done = def2.map([](const string& str) {
    print_line("resolved value: $", str);
    return bee::ok();
  });

  ivar1->resolve("hello");

  co_return co_await done;
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
      return format("outer:'$' inner:'$'", str, inner_str);
    });
  });

  def2.iter([](const string& str) { print_line("resolved value: $", str); });

  scheduler.wait_until([]() { return true; });
  print_line("Before resolving anything");

  ivar1->resolve("hello");
  scheduler.wait_until([]() { return true; });

  print_line("Before resolving def1");

  ivar_inner->resolve("inner string");
  scheduler.wait_until([]() { return true; });

  print_line("After resolving def_inner");

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
        return format("outer:'$' inner:'$'", str, inner_str);
      });
  });

  def2.iter([](const string& str) { print_line("resolved value: $", str); });

  scheduler.wait_until([]() { return true; });
  print_line("Before resolving anything");

  ivar_inner->resolve("inner string");
  scheduler.wait_until([]() { return true; });

  print_line("Before resolving def_inner");

  ivar1->resolve("hello");
  scheduler.wait_until([]() { return true; });

  print_line("After resolving def1");

  scheduler.wait_until([]() { return true; });
}

CORO_TEST(bind3)
{
  auto ivar1 = Ivar<string>::create();
  auto def1 = ivar_value(ivar1);

  auto ivar_inner = Ivar<string>::create();
  auto def_inner = ivar_value(ivar_inner);

  auto done =
    def1
      .bind([def_inner](const string& str) -> Deferred<string> {
        return def_inner.map([str = std::move(str)](const string& inner_str) {
          return format("outer:'$' inner:'$'", str, inner_str);
        });
      })
      .map([](const string& str) {
        print_line("resolved value: $", str);
        return bee::unit;
      });

  ivar_inner->resolve("inner string");
  ivar1->resolve("hello");

  co_return co_await done;
}

CORO_TEST(compile_error)
{
  auto ivar = Ivar<bee::Unit>::create();
  auto out = ivar_value(ivar).bind(
    [](bee::Unit) -> Deferred<bee::Unit> { return bee::unit; });

  ivar->resolve(bee::unit);
  co_return co_await out;
}

} // namespace
} // namespace async
