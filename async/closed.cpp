#include "closed.hpp"

#include "deferred_awaitable.hpp"
#include "ivar_multi.hpp"

#include "bee/error.hpp"

namespace async {

Closed::Closed() noexcept
    : _close_requested(false),
      _on_close_ivar(IvarMulti<bee::OrError<bee::Unit>>::create())
{}

Closed::~Closed() {}

Task<bee::OrError<bee::Unit>> Closed::closed()
{
  co_return co_await _on_close_ivar->deferred_value();
}

Task<bee::Unit> Closed::close()
{
  if (_close_requested) {
    co_await closed();
    co_return bee::unit;
  }
  _close_requested = true;

  co_await close_impl();
  _on_close_ivar->resolve(bee::ok());

  co_return bee::unit;
}

} // namespace async
