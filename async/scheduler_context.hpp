#pragma once

#include "scheduler.hpp"

namespace async {

struct SchedulerContext {
 public:
  ~SchedulerContext();

  SchedulerContext(const SchedulerContext&) = delete;
  SchedulerContext(SchedulerContext&&);

  SchedulerContext& operator=(const SchedulerContext&) = delete;
  SchedulerContext& operator=(SchedulerContext&&) = delete;

  static Scheduler& scheduler();

  static bee::OrError<SchedulerContext> create(
    std::unique_ptr<Scheduler>&& scheduler);

 private:
  explicit SchedulerContext(std::unique_ptr<Scheduler>&& scheduler);

  std::unique_ptr<Scheduler> _scheduler;
};

template <class F> void schedule(F&& callback)
{
  SchedulerContext::scheduler().schedule(std::forward<F>(callback));
}

template <class F> TimedTaskId after(const bee::Span& span, F&& callback)
{
  return SchedulerContext::scheduler().after(
    span, [callback = std::forward<F>(callback)]() { callback(); });
}

void cancel(TimedTaskId task_id);

bee::OrError<> add_fd(
  const bee::FD::shared_ptr& fd, std::function<void()>&& callback);

bee::OrError<> remove_fd(const bee::FD::shared_ptr& fd);

} // namespace async
