#pragma once

#include "task.hpp"

namespace async {

struct RunScheduler {
 public:
  static void run(std::function<async::Task<>()>&& test);

  static void run(
    std::function<async::Task<>()>&& test, SchedulerContext&& ctx);
};

} // namespace async
