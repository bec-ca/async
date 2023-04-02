#pragma once

#include "process_manager.hpp"

namespace async {

struct ProcessManagerGen {
  static bee::OrError<ProcessManager::ptr> create();
};

} // namespace async
