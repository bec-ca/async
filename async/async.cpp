#include "async.hpp"

using bee::Span;

namespace async {

Deferred<bee::Unit> after(const Span& span)
{
  auto ivar = Ivar<bee::Unit>::create();
  after(span, [ivar]() { ivar->resolve(bee::unit); });
  return ivar_value(ivar);
}

Deferred<bee::OrError<bee::Unit>> repeat(
  int times, const std::function<Deferred<bee::OrError<bee::Unit>>()>& f)
{
  if (times <= 0) { return bee::unit; }
  return f().bind(
    [f, times](const bee::OrError<bee::Unit>& result)
      -> Deferred<bee::OrError<bee::Unit>> {
      if (result.is_error()) {
        return result;
      } else {
        return repeat(times - 1, f);
      }
    });
}

} // namespace async
