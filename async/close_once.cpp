#include "close_once.hpp"

namespace async {

CloseOnce::CloseOnce() : _once([this]() { return this->_close(); }) {}

CloseOnce::~CloseOnce() {}

Task<bee::Unit> CloseOnce::close() { co_return co_await _once(); }

} // namespace async
