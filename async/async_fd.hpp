#pragma once

#include "async.hpp"
#include "ivar_multi.hpp"
#include "task.hpp"

#include "bee/data_buffer.hpp"
#include "bee/error.hpp"
#include "bee/file_descriptor.hpp"

#include <memory>

namespace async {

struct AsyncFD : public std::enable_shared_from_this<AsyncFD> {
  using ptr = std::shared_ptr<AsyncFD>;

  using ready_callback = std::function<void()>;

  ~AsyncFD();

  AsyncFD(const AsyncFD& other) = delete;
  AsyncFD(AsyncFD&&) = delete;

  bool close();

  bool is_closed() const;

  Deferred<bee::OrError<bee::Unit>> flushed();

  Task<bee::Unit> closed();

  static bee::OrError<ptr> of_fd(
    bee::FileDescriptor::shared_ptr&& fd, bool is_socket);

  bee::OrError<bee::Unit> write(std::string&& data);
  bee::OrError<bee::Unit> write(const std::string& data);

  bee::OrError<bee::Unit> write(bee::DataBuffer&& buffer);

  bee::OrError<bee::ReadResult> read(bee::DataBuffer& buffer);

  int int_fd() const;

  bee::OrError<std::optional<ptr>> accept();

  void set_ready_callback(ready_callback&& ready_callback);

 private:
  explicit AsyncFD(bee::FileDescriptor::shared_ptr&& fd, bool is_socket);

  bee::OrError<size_t> _write(const std::byte* data, size_t size);
  bee::OrError<bee::ReadResult> _read(bee::DataBuffer& buf);
  void _handle_ready();

  bee::OrError<bee::Unit> _maybe_write();

  bee::FileDescriptor::shared_ptr _fd;
  ready_callback _ready_callback;
  bool _is_socket;

  bee::DataBuffer _outgoing;

  IvarMulti<bee::OrError<bee::Unit>>::ptr _flushed_ivar;
  IvarMulti<bee::Unit>::ptr _closed_ivar;
};

struct DataPipe {
  ~DataPipe();

  AsyncFD::ptr read_fd;
  AsyncFD::ptr write_fd;

  void close();

  static bee::OrError<DataPipe> create();
};

} // namespace async
