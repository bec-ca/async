#include "async_process.hpp"

#include "bee/sub_process.hpp"
#include "process_manager.hpp"
#include "process_manager_gen.hpp"
#include "process_manager_unix.hpp"
#include "scheduler_context.hpp"

#include "bee/os.hpp"

using bee::SubProcess;

namespace async {
namespace {

bee::OrError<ProcessManager::ptr> create_manager()
{
  if constexpr (bee::RunningOS == bee::OS::Linux) {
    return ProcessManagerUnix::create();
  } else if constexpr (bee::RunningOS == bee::OS::Macos) {
    return ProcessManagerGen::create();
  }
}

const ProcessManager::ptr& get_process_manager_unix()
{
  static ProcessManager::ptr manager;
  if (manager == nullptr) {
    must_assign(manager, create_manager());

    auto& scheduler = SchedulerContext::scheduler();
    scheduler.on_exit([manager = manager]() { manager->close(); });
  }
  return manager;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// ProcessManager
//

bee::OrError<SubProcess::ptr> AsyncProcess::spawn_process(
  const SubProcess::CreateProcessArgs& args, on_exit_callback&& on_exit)
{
  return get_process_manager_unix()->spawn_process(args, std::move(on_exit));
}

} // namespace async
