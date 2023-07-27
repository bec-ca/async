#pragma once

#include "scheduler_context.hpp"

#include "bee/error.hpp"

namespace async {

struct SchedulerSelector {
  static bee::OrError<SchedulerContext> create_context();
};

} // namespace async
