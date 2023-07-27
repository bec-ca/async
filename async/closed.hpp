#pragma once

#include "closeable.hpp"
#include "ivar_multi.hpp"

namespace async {

struct Closed : public async::Closeable {
 public:
  Closed() noexcept;
  virtual ~Closed();
  virtual Task<> close() override;
  virtual Task<bee::OrError<>> closed();

 protected:
  virtual async::Task<> close_impl() = 0;

 private:
  bool _close_requested;

  IvarMulti<bee::OrError<>>::ptr _on_close_ivar;
};

} // namespace async
