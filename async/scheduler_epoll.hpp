#pragma once

#ifndef __APPLE___

#include "scheduler.hpp"
#include "scheduler_context.hpp"

namespace async {

struct SchedulerEpoll : public Scheduler {
 public:
  static bee::OrError<ptr> create_direct();

  static bee::OrError<SchedulerContext> create_context();
};

} // namespace async

#endif
