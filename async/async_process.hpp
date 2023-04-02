#pragma once

#include "bee/error.hpp"
#include "bee/sub_process.hpp"
#include "process_manager.hpp"

#include <functional>
#include <map>
#include <memory>

namespace async {

struct AsyncProcess {
 public:
  using on_exit_callback = ProcessManager::on_exit_callback;

  static bee::OrError<bee::SubProcess::ptr> spawn_process(
    const bee::SubProcess::CreateProcessArgs& args, on_exit_callback&& on_exit);
};

} // namespace async
