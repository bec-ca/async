#include "pipe.hpp"
#include "testing.hpp"

namespace async {
namespace {

ASYNC_TEST(basic)
{
  auto pipe = Pipe<int>::create();

  auto task = schedule_task([&]() { return pipe->iter([](int v) { P(v); }); });

  pipe->push(5);
  pipe->push(6);
  pipe->close();

  co_await task;
}

ASYNC_TEST(map)
{
  auto pipe1 = Pipe<int>::create();

  auto pipe2 = pipe1->map<int>([](int v) { return v * v; });

  auto task = schedule_task([&]() { return pipe2->iter([](int v) { P(v); }); });

  pipe1->push(5);
  pipe1->push(6);
  pipe1->close();

  co_await task;
}

ASYNC_TEST(destroy_while_iterating)
{
  auto pipe = Pipe<int>::create();

  auto task = schedule_task([&]() { return pipe->iter([](int v) { P(v); }); });

  pipe->push(6);
  co_return;
}

} // namespace
} // namespace async
