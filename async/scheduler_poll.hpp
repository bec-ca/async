#pragma once

#include <functional>
#include <map>
#include <queue>
#include <set>

#include "scheduler.hpp"
#include "scheduler_context.hpp"
#include "socket.hpp"

#include "bee/fd.hpp"
#include "bee/span.hpp"
#include "bee/time.hpp"

namespace async {

struct SchedulerPoll : public Scheduler {
 public:
  using ptr = std::unique_ptr<SchedulerPoll>;

  static bee::OrError<SchedulerContext> create_context();

  virtual ~SchedulerPoll();

  SchedulerPoll(const SchedulerPoll&) = delete;
  SchedulerPoll(SchedulerPoll&&) = default;

  virtual bee::OrError<> add_fd(
    const bee::FD::shared_ptr& fd, std::function<void()>&& callback);

  virtual bee::OrError<> remove_fd(const bee::FD::shared_ptr& fd);

  virtual bee::OrError<> wait(bee::Span timeout);

  virtual void schedule(std::function<void()>&& f);

  virtual void close();

  virtual bee::OrError<> wait_until(const std::function<bool()>& stop);

  virtual TimedTaskId after(
    const bee::Span& span, std::function<void()>&& callback);

  virtual void cancel(TimedTaskId task_id);

  virtual void on_exit(std::function<void()>&& on_exit);

 private:
  SchedulerPoll();

  bee::OrError<> _add_fd(const bee::FD& fd, std::function<void()> callback);

  std::map<
    std::weak_ptr<bee::FD>,
    std::function<void()>,
    std::owner_less<std::weak_ptr<bee::FD>>>
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
