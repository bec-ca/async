#pragma once

#include <memory>

#include "async.hpp"
#include "ivar_multi.hpp"
#include "task.hpp"

#include "bee/data_buffer.hpp"
#include "bee/error.hpp"
#include "bee/fd.hpp"

namespace async {

struct AsyncFD : public std::enable_shared_from_this<AsyncFD> {
  using ptr = std::shared_ptr<AsyncFD>;

  using ready_callback = std::function<void()>;

  static bee::OrError<ptr> of_fd(const bee::FD::shared_ptr& fd, bool is_socket);

  AsyncFD(const AsyncFD& other) = delete;
  AsyncFD(AsyncFD&&) = delete;

  ~AsyncFD();

  bool close();

  bool is_closed() const;

  [[nodiscard]] Task<bee::OrError<>> flushed();

  [[nodiscard]] Task<> closed();

  [[nodiscard]] bee::OrError<> write(std::string&& data);
  [[nodiscard]] bee::OrError<> write(const std::string& data);
  [[nodiscard]] bee::OrError<> write(bee::DataBuffer&& buffer);

  [[nodiscard]] bee::OrError<bee::ReadResult> read(bee::DataBuffer& buffer);

  [[nodiscard]] Task<bee::OrError<bee::ReadResult>> read_async(
    bee::DataBuffer& buffer);

  int int_fd() const;

  [[nodiscard]] bee::OrError<std::optional<ptr>> accept();

  void set_ready_callback(ready_callback&& ready_callback);

 private:
  explicit AsyncFD(const bee::FD::shared_ptr& fd, bool is_socket);

  bee::OrError<size_t> _write(const std::byte* data, size_t size);
  bee::OrError<bee::ReadResult> _read(bee::DataBuffer& buf);
  void _handle_ready();

  bee::OrError<> _maybe_write();

  bee::FD::shared_ptr _fd;
  ready_callback _ready_callback;
  bool _is_socket;

  Ivar<>::ptr _wait_ready;

  bee::DataBuffer _outgoing;

  IvarMulti<bee::OrError<>>::ptr _flushed_ivar;
  IvarMulti<>::ptr _closed_ivar;
};

struct DataPipe {
  ~DataPipe();

  AsyncFD::ptr read_fd;
  AsyncFD::ptr write_fd;

  void close();

  static bee::OrError<DataPipe> create();
};

} // namespace async
