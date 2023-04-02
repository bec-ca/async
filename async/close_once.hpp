#pragma once

#include "once.hpp"
#include "task.hpp"

namespace async {

struct CloseOnce {
 public:
  CloseOnce();

  virtual ~CloseOnce();

  virtual Task<bee::Unit> close();

 protected:
  virtual Task<bee::Unit> _close() = 0;

 private:
  Once<bee::Unit> _once;
};

} // namespace async
