#include "out_thread.hpp"
#include "testing.hpp"

#include "bee/format_optional.hpp"

namespace async {
namespace {

void out_thread_runner(OutThread<int, int>::QueuePair& queue_pair)
{
  for (auto input : queue_pair.input_queue) { queue_pair.send(input * input); }
}

ASYNC_TEST(basic)
{
  must(out_thread, (OutThread<int, int>::create(out_thread_runner, 1)));

  out_thread->send(5);
  out_thread->send(9);

  auto print_results = [&]() -> Task<> {
    P("printer started");
    while (true) {
      auto value = co_await out_thread->receive();
      if (!value.has_value()) break;
      P("result: $", value);
    }
    P("printer exited");
  };

  auto print_task = print_results();

  out_thread->close();

  co_await print_task;
}

ASYNC_TEST(dont_close)
{
  must(ot, (OutThread<int, int>::create(out_thread_runner, 1)));
  [[maybe_unused]] auto v = ot;
  co_return;
}

ASYNC_TEST(out_thread_exists)
{
  must(
    out_thread, (OutThread<int, int>::create([](const auto&) { return; }, 1)));

  while (true) {
    auto value = co_await out_thread->receive();
    if (!value.has_value()) break;
    P("got unexpecte value");
  }
}

} // namespace
} // namespace async
