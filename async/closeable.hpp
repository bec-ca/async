#pragma once

#include "task.hpp"

namespace async {

template <class F>
concept closeable = requires(F f) {
  {
    f.close()
  } -> task;
};

struct Closeable {
 public:
  virtual ~Closeable();

  virtual Task<> close() = 0;
};

} // namespace async
