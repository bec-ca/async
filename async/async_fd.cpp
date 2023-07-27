#include "async_fd.hpp"

#include "async.hpp"
#include "deferred_awaitable.hpp"

using bee::FD;
using std::nullopt;
using std::optional;
using std::string;
using std::weak_ptr;

namespace async {

AsyncFD::AsyncFD(const FD::shared_ptr& fd, bool is_socket)
    : _fd(fd), _is_socket(is_socket)
{}

AsyncFD::~AsyncFD() { close(); }

bee::OrError<AsyncFD::ptr> AsyncFD::of_fd(
  const FD::shared_ptr& fd, bool is_socket)
{
  bail_unit(fd->set_blocking(false));

  auto sock = ptr(new AsyncFD(fd, is_socket));

  bail_unit(add_fd(fd, [weak = weak_ptr(sock)] {
    if (auto ptr = weak.lock()) { ptr->_handle_ready(); }
  }));

  return sock;
}

void AsyncFD::_handle_ready()
{
  if (_fd == nullptr) { assert(false && "Got data after close"); }

  // TODO: handle error better
  must_unit(_maybe_write());

  if (_ready_callback != nullptr) { _ready_callback(); }
  if (_wait_ready != nullptr) { _wait_ready->fill(); }
}

void AsyncFD::set_ready_callback(ready_callback&& ready_callback)
{
  _ready_callback = ready_callback;
}

bee::OrError<> AsyncFD::write(const string& data)
{
  _outgoing.write(data);
  return _maybe_write();
}

bee::OrError<> AsyncFD::write(string&& data)
{
  _outgoing.write(std::move(data));
  return _maybe_write();
}

bee::OrError<> AsyncFD::write(bee::DataBuffer&& data)
{
  _outgoing.write(std::move(data));
  return _maybe_write();
}

bee::OrError<bee::ReadResult> AsyncFD::read(bee::DataBuffer& buf)
{
  return _read(buf);
}

bee::OrError<> AsyncFD::_maybe_write()
{
  size_t bytes_sent = 0;
  for (auto& block : _outgoing) {
    if (block.empty()) { continue; }
    size_t block_bytes_sent = 0;
    while (block_bytes_sent < block.size()) {
      bail(
        ret,
        _write(
          block.data() + block_bytes_sent, block.size() - block_bytes_sent));
      if (ret == 0) { break; }
      block_bytes_sent += ret;
    }
    bytes_sent += block_bytes_sent;
    if (block_bytes_sent < block.size()) { break; }
  }
  _outgoing.consume(bytes_sent);
  if (_outgoing.empty() && _flushed_ivar != nullptr) {
    _flushed_ivar->fill(bee::ok());
    _flushed_ivar = nullptr;
  }
  return bee::ok();
}

bee::OrError<size_t> AsyncFD::_write(const std::byte* data, size_t size)
{
  if (_fd == nullptr) { shot("FD already closed"); }
  if (_is_socket) {
    return _fd->send(data, size);
  } else {
    return _fd->write(data, size);
  }
}

Task<bee::OrError<bee::ReadResult>> AsyncFD::read_async(bee::DataBuffer& buf)
{
  if (_fd == nullptr) { co_return bee::Error("FD already closed"); }
  if (_wait_ready != nullptr) {
    co_return bee::Error("Cannot have two concurrent reads");
  }
  while (true) {
    co_bail(result, _read(buf));
    if (result.bytes_read() > 0 || result.is_eof()) { co_return result; }
    _wait_ready = Ivar<>::create();
    co_await _wait_ready;
    _wait_ready = nullptr;
  }
}

bee::OrError<bee::ReadResult> AsyncFD::_read(bee::DataBuffer& buf)
{
  if (_fd == nullptr) { shot("FD already closed"); }
  if (_is_socket) {
    return _fd->recv_all_available(buf);
  } else {
    return _fd->read_all_available(buf);
  }
}

bool AsyncFD::close()
{
  if (_fd == nullptr) { return false; }
  remove_fd(_fd);
  auto ret = _fd->close();
  _fd = nullptr;
  if (_flushed_ivar != nullptr) {
    _flushed_ivar->fill(bee::ok());
    _flushed_ivar = nullptr;
  }
  if (_closed_ivar != nullptr && !_closed_ivar->is_determined()) {
    _closed_ivar->fill(bee::unit);
  }
  return ret;
}

bool AsyncFD::is_closed() const { return _fd == nullptr; }

int AsyncFD::int_fd() const { return _fd->int_fd(); }

bee::OrError<optional<AsyncFD::ptr>> AsyncFD::accept()
{
  bail(fd, _fd->accept());
  if (!fd.has_value()) { return nullopt; }
  return of_fd(std::move(*fd).to_shared(), true);
}

Task<bee::OrError<>> AsyncFD::flushed()
{
  if (_outgoing.empty()) { co_return bee::ok(); }
  if (_flushed_ivar == nullptr) {
    _flushed_ivar = IvarMulti<bee::OrError<>>::create();
  }
  co_return co_await _flushed_ivar->deferred_value();
}

Task<> AsyncFD::closed()
{
  if (is_closed()) { co_return; }
  if (_closed_ivar == nullptr) { _closed_ivar = IvarMulti<>::create(); }
  co_await _closed_ivar->deferred_value();
}

////////////////////////////////////////////////////////////////////////////////
// Pipe
//

DataPipe::~DataPipe() {}

void DataPipe::close()
{
  if (read_fd != nullptr) {
    read_fd->close();
    read_fd = nullptr;
  }
  if (write_fd != nullptr) {
    write_fd->close();
    write_fd = nullptr;
  }
}

bee::OrError<DataPipe> DataPipe::create()
{
  bail(sync_pipe, bee::Pipe::create());
  bail(read_fd, AsyncFD::of_fd(std::move(sync_pipe.read_fd), false));
  bail(write_fd, AsyncFD::of_fd(std::move(sync_pipe.write_fd), false));
  return DataPipe{
    .read_fd = std::move(read_fd),
    .write_fd = std::move(write_fd),
  };
}

} // namespace async
