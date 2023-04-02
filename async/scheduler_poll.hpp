#pragma once

#include "bee/file_descriptor.hpp"
#include "bee/span.hpp"
#include "bee/time.hpp"
#include "scheduler.hpp"
#include "scheduler_context.hpp"
#include "socket.hpp"

#include <functional>
#include <map>
#include <queue>
#include <set>

namespace async {

struct SchedulerPoll : public Scheduler {
 public:
  using ptr = std::unique_ptr<SchedulerPoll>;

  static bee::OrError<SchedulerContext> create_context();

  virtual ~SchedulerPoll();

  SchedulerPoll(const SchedulerPoll&) = delete;
  SchedulerPoll(SchedulerPoll&&) = default;

  virtual bee::OrError<bee::Unit> add_fd(
    const bee::FileDescriptor::shared_ptr& fd,
    std::function<void()>&& callback);

  virtual bee::OrError<bee::Unit> remove_fd(
    const bee::FileDescriptor::shared_ptr& fd);

  virtual bee::OrError<bee::Unit> wait(bee::Span timeout);

  virtual void schedule(std::function<void()>&& f);

  virtual void close();

  virtual bee::OrError<bee::Unit> wait_until(const std::function<bool()>& stop);

  virtual TimedTaskId after(
    const bee::Span& span, std::function<void()>&& callback);

  virtual void cancel(TimedTaskId task_id);

  virtual void on_exit(std::function<void()>&& on_exit);

 private:
  SchedulerPoll();

  bee::OrError<bee::Unit> _add_fd(
    const bee::FileDescriptor& fd, std::function<void()> callback);

  std::map<
    std::weak_ptr<bee::FileDescriptor>,
    std::function<void()>,
    std::owner_less<std::weak_ptr<bee::FileDescriptor>>>
    _callbacks;

  std::queue<std::function<void()>> _task_queue;

  struct TimedTask {
    bee::Time when;
    std::function<void()> callback;

    bool operator<(const TimedTask& other) const;
  };

  std::priority_queue<TimedTask> _timed_task_queue;

  void _run_tasks_until_empty();

  std::vector<std::function<void()>> _on_exit;
};

} // namespace async
