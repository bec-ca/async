#include "process_manager_unix.hpp"

#include "bee/sub_process.hpp"
#include "process_manager.hpp"
#include "scheduler_context.hpp"

#include "bee/signal.hpp"

#include <csignal>
#include <cstring>
#include <map>

using bee::SubProcess;
using std::make_shared;
using std::map;
using std::weak_ptr;

namespace async {

#ifdef __APPLE__

bee::OrError<ProcessManager::ptr> ProcessManagerUnix::create()
{
  return bee::Error("Not supported on apple");
}

#else

namespace {

struct ProcessManagerUnixImpl : public ProcessManager {
 public:
  virtual ~ProcessManagerUnixImpl();

  virtual bee::OrError<SubProcess::ptr> spawn_process(
    const SubProcess::CreateProcessArgs& args, on_exit_callback&& on_exit);

  bee::OrError<bee::Unit> check_children();

  ProcessManagerUnixImpl(bee::FileDescriptor&& fd);

  static bee::OrError<ptr> create();

  virtual void close();

 private:
  map<SubProcess::ptr, std::function<void(int exit_status)>> _callbacks;

  bee::FileDescriptor::shared_ptr _fd;
};

ProcessManagerUnixImpl::ProcessManagerUnixImpl(bee::FileDescriptor&& fd)
    : _fd(std::move(fd).to_shared())
{}

ProcessManagerUnixImpl::~ProcessManagerUnixImpl() {}

bee::OrError<ProcessManager::ptr> ProcessManagerUnixImpl::create()
{
  bail(fd, bee::Signal::create_signal_fd(bee::SignalCode::SigChld));

  auto manager = make_shared<ProcessManagerUnixImpl>(std::move(fd));
  auto manager_weak = weak_ptr(manager);

  auto& scheduler = SchedulerContext::scheduler();

  scheduler.add_fd(manager->_fd, [manager_weak]() {
    auto manager = manager_weak.lock();
    if (manager != nullptr) { must_unit(manager->check_children()); }
  });

  return manager;
}

bee::OrError<SubProcess::ptr> ProcessManagerUnixImpl::spawn_process(
  const SubProcess::CreateProcessArgs& args, on_exit_callback&& on_exit)
{
  bail(proc, SubProcess::spawn(args));

  _callbacks.emplace(proc, std::move(on_exit));

  return proc;
}

bee::OrError<bee::Unit> ProcessManagerUnixImpl::check_children()
{
  while (true) {
    char buffer[1024];
    bail(ret, _fd->read(reinterpret_cast<std::byte*>(buffer), sizeof(buffer)));
    if (ret.bytes_read() == 0) break;
  }

  while (true) {
    bail(status, SubProcess::wait_any(false));
    if (!status.has_value()) { break; }

    auto it = _callbacks.find(status->proc);
    if (it == _callbacks.end()) {
      return bee::Error::format(
        "Got status changed on unknown pid: $", status->proc->pid().to_int());
    }
    it->second(status->exit_status);
    _callbacks.erase(it);
  }

  return bee::ok();
}

void ProcessManagerUnixImpl::close()
{
  if (!_callbacks.empty()) {
    assert(false && "There are still one or more child process running");
  }
  _fd->close();
}

} // namespace

bee::OrError<ProcessManager::ptr> ProcessManagerUnix::create()
{
  return ProcessManagerUnixImpl::create();
}

#endif

} // namespace async
