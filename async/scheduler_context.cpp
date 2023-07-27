#include "scheduler_context.hpp"

#include <stdexcept>
#include <thread>

#include "scheduler.hpp"

namespace async {
namespace {

Scheduler*& singleton_scheduler()
{
  static Scheduler* scheduler = nullptr;
  return scheduler;
}

std::thread::id& creator_id()
{
  static std::thread::id id;
  return id;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// SchedulerContext
//

SchedulerContext::SchedulerContext(std::unique_ptr<Scheduler>&& scheduler)
    : _scheduler(std::move(scheduler))
{}

SchedulerContext::SchedulerContext(SchedulerContext&& other)
    : _scheduler(other._scheduler.release())
{}

SchedulerContext::~SchedulerContext()
{
  if (_scheduler != nullptr) {
    assert(singleton_scheduler() == _scheduler.get());
    _scheduler->close();
    singleton_scheduler() = nullptr;
  }
}

bee::OrError<SchedulerContext> SchedulerContext::create(
  std::unique_ptr<Scheduler>&& scheduler)
{
  if (singleton_scheduler() != nullptr) {
    return bee::Error("Already initialized by other SchedulerContext instance");
  }
  singleton_scheduler() = scheduler.get();
  creator_id() = std::this_thread::get_id();

  return SchedulerContext(std::move(scheduler));
}

Scheduler& SchedulerContext::scheduler()
{
  auto scheduler = singleton_scheduler();
  if (scheduler == nullptr) {
    throw std::runtime_error("No scheduler initialized");
  }
  if (std::this_thread::get_id() != creator_id()) {
    throw std::runtime_error("Scheduler called from the wrong thread");
  }
  return *scheduler;
}

void cancel(TimedTaskId task_id)
{
  SchedulerContext::scheduler().cancel(task_id);
}

bee::OrError<> add_fd(
  const bee::FD::shared_ptr& fd, std::function<void()>&& callback)
{
  return SchedulerContext::scheduler().add_fd(fd, std::move(callback));
}

bee::OrError<> remove_fd(const bee::FD::shared_ptr& fd)
{
  return SchedulerContext::scheduler().remove_fd(fd);
}

} // namespace async
