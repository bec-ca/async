#pragma once

#include "bee/error.hpp"
#include "scheduler_context.hpp"

namespace async {

struct SchedulerSelector {
  static bee::OrError<SchedulerContext> create_context();
};

} // namespace async
