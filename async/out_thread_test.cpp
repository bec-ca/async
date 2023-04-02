#include "out_thread.hpp"

#include "bee/format_optional.hpp"
#include "testing.hpp"

using bee::print_line;

namespace async {

namespace {

void out_thread_runner(OutThread<int, int>::QueuePair& queue_pair)
{
  for (auto input : queue_pair.input_queue) { queue_pair.send(input * input); }
}

CORO_TEST(basic)
{
  co_bail(out_thread, (OutThread<int, int>::create(out_thread_runner, 1)));

  out_thread->send(5);
  out_thread->send(9);

  auto print_results = [&]() -> Task<bee::Unit> {
    print_line("printer started");
    while (true) {
      auto value = co_await out_thread->receive();
      if (!value.has_value()) break;
      print_line("result: $", value);
    }
    print_line("printer exited");
    co_return bee::unit;
  };

  auto print_task = print_results();

  out_thread->close();

  co_await print_task;

  co_return bee::unit;
}

CORO_TEST(dont_close)
{
  co_bail(out_thread, (OutThread<int, int>::create(out_thread_runner, 1)));

  co_return bee::unit;
}

CORO_TEST(out_thread_exists)
{
  co_bail(
    out_thread, (OutThread<int, int>::create([](const auto&) { return; }, 1)));

  while (true) {
    auto value = co_await out_thread->receive();
    if (!value.has_value()) break;
    print_line("got unexpecte value");
  }

  co_return bee::unit;
}

} // namespace

} // namespace async
