#pragma once

#include <functional>
#include <memory>

#include "bee/sub_process.hpp"

namespace async {

struct ProcessManager {
 public:
  using ptr = std::shared_ptr<ProcessManager>;
  using on_exit_callback = std::function<void(int exit_status)>;

  virtual bee::OrError<bee::SubProcess::ptr> spawn_process(
    const bee::SubProcess::CreateProcessArgs& args,
    on_exit_callback&& on_exit) = 0;

  virtual void close() = 0;
};

} // namespace async
