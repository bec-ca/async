#include "scheduler_selector.hpp"

#include "scheduler_epoll.hpp"
#include "scheduler_poll.hpp"

#include "bee/os.hpp"
#include "bee/signal.hpp"

namespace async {

bee::OrError<SchedulerContext> SchedulerSelector::create_context()
{
  bail_unit(bee::Signal::block_signal(bee::SignalCode::SigPipe));

  if constexpr (bee::RunningOS == bee::OS::Linux) {
    return SchedulerEpoll::create_context();
  } else if constexpr (bee::RunningOS == bee::OS::Macos) {
    return SchedulerPoll::create_context();
  }
}

} // namespace async
