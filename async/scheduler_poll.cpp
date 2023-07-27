#include "scheduler_poll.hpp"

#include <cstring>
#include <map>

#include <poll.h>
#include <sys/errno.h>

#include "async.hpp"
#include "scheduler.hpp"

#include "bee/fd.hpp"

using bee::FD;
using bee::Span;
using bee::Time;
using std::function;
using std::vector;
using std::weak_ptr;

// TODO: remove code duplication between this and the epoll based scheduler

namespace async {

SchedulerPoll::SchedulerPoll() {}

void SchedulerPoll::schedule(function<void()>&& f)
{
  _task_queue.emplace(std::move(f));
}

bee::OrError<SchedulerContext> SchedulerPoll::create_context()
{
  auto poll = ptr(new SchedulerPoll());
  return SchedulerContext::create(std::move(poll));
}

bee::OrError<> SchedulerPoll::add_fd(
  const FD::shared_ptr& fd, function<void()>&& callback)
{
  _callbacks.emplace(fd, std::move(callback));

  return bee::ok();
}

bee::OrError<> SchedulerPoll::remove_fd(const FD::shared_ptr& fd)
{
  _callbacks.erase(fd);
  return bee::ok();
}

const Span max_timeout = Span::of_seconds(60);

bee::OrError<> SchedulerPoll::wait(Span timeout)
{
  _run_tasks_until_empty();

  auto now = Time::monotonic();
  while (!_timed_task_queue.empty() && _timed_task_queue.top().when <= now) {
    auto top = std::move(_timed_task_queue.top());
    _timed_task_queue.pop();
    top.callback();
  }

  if (!_timed_task_queue.empty()) {
    auto remaining = _timed_task_queue.top().when.diff(now);
    if (remaining < timeout) { timeout = remaining; }
  }

  // clear up collected fds
  for (auto it = _callbacks.begin(); it != _callbacks.end(); it++) {
    if (it->first.expired()) {
      auto to_delete = it;
      it++;
      _callbacks.erase(to_delete);
    }
  }

  vector<pollfd> poll_fds;
  vector<weak_ptr<FD>> fds;
  for (const auto& fdp : _callbacks) {
    auto fd = fdp.first.lock();
    short events = POLLIN;
    if (fd->is_write_blocked()) { events |= POLLOUT; }
    if (fd == nullptr) { continue; }
    poll_fds.push_back({.fd = fd->int_fd(), .events = events, .revents = 0});
    fds.push_back(fdp.first);
  }

  timeout = std::clamp(timeout, Span::zero(), max_timeout);

  int ret = poll(poll_fds.data(), poll_fds.size(), timeout.to_millis());

  if (ret == -1) {
    if (errno != EINTR) {
      return bee::Error::fmt("Failed to wait to epoll: $", strerror(errno));
    }
  } else {
    for (int i = 0; i < int(poll_fds.size()); i++) {
      auto& pollfd = poll_fds.at(i);
      if (pollfd.revents == 0) continue;
      auto fd = fds.at(i);
      schedule([this, fd]() {
        auto it = _callbacks.find(fd);
        if (it == _callbacks.end()) return;
        it->second();
      });
    }
  }

  _run_tasks_until_empty();

  return bee::ok();
}

bee::OrError<> SchedulerPoll::wait_until(const function<bool()>& stop)
{
  auto timeout = Span::of_seconds(1);
  while (true) {
    _run_tasks_until_empty();
    if (stop()) { break; }
    bail_unit(wait(timeout));
  }

  return bee::ok();
}

void SchedulerPoll::close()
{
  for (const auto& f : _on_exit) { f(); }
  _callbacks.clear();
}

SchedulerPoll::~SchedulerPoll() {}

void SchedulerPoll::_run_tasks_until_empty()
{
  while (!_task_queue.empty()) {
    _task_queue.front()();
    _task_queue.pop();
  }
}

TimedTaskId SchedulerPoll::after(
  const Span& span, std::function<void()>&& callback)
{
  auto when = Time::monotonic() + span;
  _timed_task_queue.emplace(
    TimedTask{.when = when, .callback = std::move(callback)});
  return TimedTaskId(0);
}

void SchedulerPoll::cancel(TimedTaskId) {}

void SchedulerPoll::on_exit(std::function<void()>&& on_exit)
{
  _on_exit.push_back(std::move(on_exit));
}

////////////////////////////////////////////////////////////////////////////////
// TimedTask
//

bool SchedulerPoll::TimedTask::operator<(const TimedTask& other) const
{
  return when < other.when;
}

} // namespace async
