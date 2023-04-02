#pragma once

#include "async.hpp"
#include "command/command_builder.hpp"
#include "task.hpp"

namespace async {

command::Cmd run_async(
  command::CommandBuilder& builder,
  const std::function<Deferred<bee::OrError<bee::Unit>>()>& f);

command::Cmd run_coro(
  command::CommandBuilder& builder,
  std::function<Task<bee::OrError<bee::Unit>>()> f);

} // namespace async
