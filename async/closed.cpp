#include "closed.hpp"

#include "deferred_awaitable.hpp"
#include "ivar_multi.hpp"

#include "bee/error.hpp"

namespace async {

Closed::Closed() noexcept
    : _close_requested(false),
      _on_close_ivar(IvarMulti<bee::OrError<>>::create())
{}

Closed::~Closed() {}

Task<bee::OrError<>> Closed::closed()
{
  co_return co_await _on_close_ivar->deferred_value();
}

Task<> Closed::close()
{
  if (_close_requested) { co_await closed(); }
  _close_requested = true;

  co_await close_impl();
  _on_close_ivar->fill(bee::ok());
}

} // namespace async
