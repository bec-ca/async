#pragma once

#include "process_manager.hpp"

namespace async {

struct ProcessManagerUnix {
  static bee::OrError<ProcessManager::ptr> create();
};

} // namespace async
