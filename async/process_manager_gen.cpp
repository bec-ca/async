#include "process_manager_gen.hpp"

#include <queue>

#include "out_thread.hpp"

#include "async/process_manager.hpp"
#include "bee/sub_process.hpp"

using bee::SubProcess;
using std::make_shared;
using std::make_unique;
using std::unique_ptr;

namespace async {

namespace {

struct WorkInput {
  using ptr = unique_ptr<WorkInput>;
  SubProcess::ptr proc;
  ProcessManager::on_exit_callback exit_callback;

  WorkInput(
    SubProcess::ptr&& proc, ProcessManager::on_exit_callback&& exit_callback)
      : proc(std::move(proc)), exit_callback(std::move(exit_callback))
  {}
};

struct WorkOutput {
  using ptr = unique_ptr<WorkOutput>;
  ProcessManager::on_exit_callback exit_callback;
  bee::OrError<> result;

  WorkOutput(
    ProcessManager::on_exit_callback&& exit_callback, bee::OrError<>&& result)
      : exit_callback(std::move(exit_callback)), result(std::move(result))
  {}
};

using OT = OutThread<WorkInput::ptr, WorkOutput::ptr>;

struct ProcessManagerGenImpl : public ProcessManager {
 public:
  ProcessManagerGenImpl(OT::ptr&& out_threads)
      : _out_threads(std::move(out_threads)),
        _handler(_out_threads->output_pipe()->iter(_handle_output))
  {}

  virtual ~ProcessManagerGenImpl() {}

  virtual bee::OrError<SubProcess::ptr> spawn_process(
    const SubProcess::CreateProcessArgs& args,
    on_exit_callback&& on_exit) override
  {
    bail(proc, SubProcess::spawn(args));
    _out_threads->send(
      make_unique<WorkInput>(std::move(proc), std::move(on_exit)));
    return nullptr;
  }

  static bee::OrError<ProcessManager::ptr> create()
  {
    bail(out_threads, OT::create(_run_thread, 16));
    return make_shared<ProcessManagerGenImpl>(std::move(out_threads));
  }

  virtual void close() override { _out_threads->close(); };

 private:
  static void _run_thread(OT::QueuePair& queue_pair)
  {
    for (auto& work : queue_pair.input_queue) {
      auto res = work->proc->wait();
      queue_pair.output_queue.push(make_unique<WorkOutput>(
        std::move(work->exit_callback), std::move(res).ignore_value()));
    }
  }

  static void _handle_output(WorkOutput::ptr&& output)
  {
    if (output->result.is_error()) {
      output->exit_callback(1);
    } else {
      output->exit_callback(0);
    }
  }

  OT::ptr _out_threads;
  Task<> _handler;
};

} // namespace

bee::OrError<ProcessManager::ptr> ProcessManagerGen::create()
{
  return ProcessManagerGenImpl::create();
}

} // namespace async
