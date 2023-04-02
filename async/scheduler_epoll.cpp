#include "scheduler_epoll.hpp"
#include "bee/file_descriptor.hpp"

#ifndef __APPLE__

#include "async.hpp"
#include "scheduler.hpp"

#include "bee/time.hpp"

#include <cstring>
#include <map>
#include <sys/epoll.h>

using bee::FileDescriptor;
using bee::Span;
using bee::Time;
using std::function;

namespace async {

namespace {

struct TimedTaskIdGenerator {
 public:
  TimedTaskId next_id() { return TimedTaskId(_next_id++); }

 private:
  uint64_t _next_id = 1;
};

const Span max_timeout = Span::of_seconds(60);

struct SchedulerEpollImpl : public Scheduler {
 public:
  using ptr = std::unique_ptr<SchedulerEpollImpl>;

  // methods
  SchedulerEpollImpl(FileDescriptor&& fd) : _epoll_fd(std::move(fd)) {}

  virtual ~SchedulerEpollImpl() {}

  SchedulerEpollImpl(const SchedulerEpollImpl&) = delete;
  SchedulerEpollImpl(SchedulerEpollImpl&&) = default;

  virtual bee::OrError<bee::Unit> add_fd(
    const FileDescriptor::shared_ptr& fd, function<void()>&& callback)
  {
    int fd_i = fd->int_fd();
    if (_fd_to_id.find(fd) != _fd_to_id.end()) {
      assert(false && "Duplicated fd");
    }

    uint32_t id = _next_id++;
    epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.u32 = id;

    int ret = epoll_ctl(_epoll_fd.int_fd(), EPOLL_CTL_ADD, fd_i, &event);
    if (ret == -1) {
      return bee::Error::format(
        "Failed to add socket to epoll: $", strerror(errno));
    }

    _callbacks.emplace(id, std::move(callback));
    _fd_to_id.emplace(fd, id);

    return bee::unit;
  }

  bee::OrError<bee::Unit> remove_fd(const FileDescriptor::shared_ptr& fd)
  {
    auto it = _fd_to_id.find(fd);
    if (it == _fd_to_id.end()) {
      return bee::Error::format("ID for fd not found");
    }
    auto id = it->second;

    _fd_to_id.erase(fd);
    _callbacks.erase(id);

    return bee::ok();
  }

  virtual void schedule(function<void()>&& f)
  {
    _primary_task_queue.emplace_back(std::move(f));
  }

  void close()
  {
    for (const auto& f : _on_exit) { f(); }
    _epoll_fd.close();
    _callbacks.clear();
    _fd_to_id.clear();
  }

  bee::OrError<bee::Unit> wait_until(const function<bool()>& stop)
  {
    do {
      _move_time();

      Span timeout = max_timeout;
      if (stop() || !_primary_task_queue.empty()) { timeout = Span::zero(); }
      bail_unit(_wait_events(timeout));
    } while (!stop() || !_primary_task_queue.empty());

    return bee::ok();
  }

  virtual TimedTaskId after(const Span& span, std::function<void()>&& callback)
  {
    auto when = Time::monotonic() + span;
    auto id = _timed_task_id_generator.next_id();
    if (when > _last_now) {
      auto it = _timed_task_queue.emplace(
        when, TimedTask{.fn = std::move(callback), .id = id});
      _timed_tasks.emplace(id, it);
    } else {
      schedule(std::move(callback));
    }
    return id;
  }

  virtual void cancel(TimedTaskId task_id)
  {
    auto it = _timed_tasks.find(task_id);
    if (it == _timed_tasks.end()) { return; }
    _timed_task_queue.erase(it->second);
    _timed_tasks.erase(it);
  }

  virtual void on_exit(std::function<void()>&& on_exit)
  {
    _on_exit.push_back(std::move(on_exit));
  }

 private:
  bee::OrError<bee::Unit> _add_fd(
    const FileDescriptor& fd, std::function<void()> callback);

  void _run_tasks_until_empty()
  {
    swap(_primary_task_queue, _secondary_task_queue);
    for (auto& t : _secondary_task_queue) { t(); }
    _secondary_task_queue.clear();
  }

  void _move_time()
  {
    auto now = Time::monotonic();
    while (!_timed_task_queue.empty()) {
      auto it = _timed_task_queue.begin();
      auto& [when, task] = *it;
      if (when > now) { break; }
      _timed_tasks.erase(task.id);
      schedule(std::move(task.fn));
      _timed_task_queue.erase(it);
    }
    _last_now = now;

    _run_tasks_until_empty();
  }

  bee::OrError<bee::Unit> _wait_events(Span timeout)
  {
    constexpr int max_events = 64;
    epoll_event events[max_events];
    if (!_timed_task_queue.empty()) {
      auto it = _timed_task_queue.begin();
      auto remaining = it->first.diff(_last_now);
      if (remaining < timeout) { timeout = remaining; }
    }

    timeout = std::clamp(timeout, Span::zero(), max_timeout);

    int ret =
      epoll_wait(_epoll_fd.int_fd(), events, max_events, timeout.to_millis());

    if (ret == -1) {
      if (errno != EINTR) {
        return bee::Error::format(
          "Failed to wait to epoll: $", strerror(errno));
      }
    } else {
      for (int i = 0; i < ret; i++) {
        uint32_t id = events[i].data.u32;
        schedule([this, id]() {
          auto it = _callbacks.find(id);
          if (it == _callbacks.end()) { return; }
          auto f = it->second;
          f();
        });
      }
    }
    return bee::ok();
  }

  // fields
  FileDescriptor _epoll_fd;
  using Id = uint32_t;

  std::unordered_map<Id, std::function<void()>> _callbacks;
  std::unordered_map<FileDescriptor::shared_ptr, Id> _fd_to_id;
  Id _next_id = 0;

  struct TimedTask {
   public:
    std::function<void()> fn;
    TimedTaskId id;
  };

  using timed_task_queue = std::multimap<Time, TimedTask>;

  TimedTaskIdGenerator _timed_task_id_generator;

  timed_task_queue _timed_task_queue;

  std::map<TimedTaskId, timed_task_queue::iterator> _timed_tasks;

  std::vector<std::function<void()>> _primary_task_queue;
  std::vector<std::function<void()>> _secondary_task_queue;

  std::vector<std::function<void()>> _on_exit;

  Time _last_now = Time::zero();
};

} // namespace

bee::OrError<Scheduler::ptr> SchedulerEpoll::create_direct()
{
  int fd = epoll_create1(EPOLL_CLOEXEC);

  if (fd == -1) {
    return bee::Error::format("Failed to create epoll: $", strerror(errno));
  }

  return std::make_unique<SchedulerEpollImpl>(FileDescriptor(fd));
}

bee::OrError<SchedulerContext> SchedulerEpoll::create_context()
{
  bail(poll, create_direct());
  return SchedulerContext::create(std::move(poll));
}

} // namespace async

#endif
