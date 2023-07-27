#include "async.hpp"

using bee::Span;

namespace async {

Deferred<> after(const Span& span)
{
  auto ivar = Ivar<>::create();
  after(span, [ivar]() { ivar->fill(); });
  return ivar_value(ivar);
}

Deferred<bee::OrError<>> repeat(
  int times, const std::function<Deferred<bee::OrError<>>()>& f)
{
  if (times <= 0) { return bee::ok(); }
  return f().bind(
    [f, times](const bee::OrError<>& result) -> Deferred<bee::OrError<>> {
      if (result.is_error()) {
        return result;
      } else {
        return repeat(times - 1, f);
      }
    });
}

} // namespace async
