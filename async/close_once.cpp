#include "close_once.hpp"

namespace async {

CloseOnce::CloseOnce()
    : _once([this]() -> Task<bee::Unit> {
        co_await this->_close();
        co_return bee::unit;
      })
{}

CloseOnce::~CloseOnce() {}

Task<> CloseOnce::close() { co_await _once(); }

} // namespace async
