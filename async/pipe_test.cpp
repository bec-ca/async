#include "pipe.hpp"

#include "testing.hpp"

using bee::print_line;

namespace async {
namespace {

CORO_TEST(basic)
{
  auto pipe = Pipe<int>::create();

  auto task = schedule_task(
    [&]() { return pipe->iter_values([](int v) { print_line(v); }); });

  pipe->push(5);
  pipe->push(6);
  pipe->close();

  co_await task;

  co_return bee::unit;
}

CORO_TEST(map)
{
  auto pipe1 = Pipe<int>::create();

  auto pipe2 = pipe1->map<int>([](int v) { return v * v; });

  auto task = schedule_task(
    [&]() { return pipe2->iter_values([](int v) { print_line(v); }); });

  pipe1->push(5);
  pipe1->push(6);
  pipe1->close();

  co_await task;

  co_return bee::unit;
}

CORO_TEST(destroy_while_iterating)
{
  auto pipe = Pipe<int>::create();

  auto task = schedule_task(
    [&]() { return pipe->iter_values([](int v) { print_line(v); }); });

  pipe->push(6);

  co_return bee::unit;
}

} // namespace
} // namespace async
