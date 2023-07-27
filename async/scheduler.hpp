#pragma once

#include <functional>

#include "bee/error.hpp"
#include "bee/fd.hpp"
#include "bee/span.hpp"

namespace async {

struct TimedTaskId {
 public:
  explicit TimedTaskId(uint64_t id) : _id(id) {}

  TimedTaskId(const TimedTaskId& other) = default;

  explicit operator uint64_t() const { return _id; }

  uint64_t to_int() const { return _id; }

  std::strong_ordering operator<=>(const TimedTaskId& other) const
  {
    return _id <=> other._id;
  }

 private:
  uint64_t _id;
};

struct Scheduler {
 public:
  using ptr = std::unique_ptr<Scheduler>;

  virtual ~Scheduler();

  virtual bee::OrError<> add_fd(
    const bee::FD::shared_ptr& fd, std::function<void()>&& callback) = 0;

  virtual bee::OrError<> remove_fd(const bee::FD::shared_ptr& fd) = 0;

  virtual void schedule(std::function<void()>&& f) = 0;

  virtual void close() = 0;

  virtual bee::OrError<> wait_until(const std::function<bool()>& stop) = 0;

  virtual TimedTaskId after(
    const bee::Span& span, std::function<void()>&& callback) = 0;

  virtual void cancel(TimedTaskId task_id) = 0;

  virtual void on_exit(std::function<void()>&& on_exit) = 0;
};

} // namespace async
