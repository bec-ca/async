#pragma once

#include "task.hpp"

namespace async {

struct RunScheduler {
 public:
  static bee::OrError<bee::Unit> run_async(
    std::function<async::Deferred<bee::OrError<bee::Unit>>()>&& test);

  static bee::OrError<bee::Unit> run_coro(
    std::function<async::Task<bee::OrError<bee::Unit>>()>&& test);

  static bee::OrError<bee::Unit> run_coro(
    std::function<async::Task<bee::OrError<bee::Unit>>()>&& test,
    SchedulerContext&& ctx);
};

} // namespace async
