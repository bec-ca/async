#pragma once

#include "async.hpp"
#include "task.hpp"

#include "command/command_builder.hpp"

namespace async {

command::Cmd command(
  command::CommandBuilder& builder, std::function<Task<bee::OrError<>>()> f);

} // namespace async
