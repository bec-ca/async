#pragma once

#include "closeable.hpp"
#include "ivar_multi.hpp"

namespace async {

struct Closed : public async::Closeable {
 public:
  Closed() noexcept;
  virtual ~Closed();
  virtual Task<bee::Unit> close() override;
  virtual Task<bee::OrError<bee::Unit>> closed();

 protected:
  virtual async::Task<bee::Unit> close_impl() = 0;

 private:
  bool _close_requested;

  IvarMulti<bee::OrError<bee::Unit>>::ptr _on_close_ivar;
};

} // namespace async
