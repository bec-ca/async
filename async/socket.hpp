#pragma once

#include "async_fd.hpp"
#include "task.hpp"

#include "bee/data_buffer.hpp"
#include "bee/error.hpp"
#include "bee/file_descriptor.hpp"

#include <memory>

namespace async {

struct IPV4 {
  uint32_t address;
};

struct IPV6 {
  uint8_t address[16];
};

using IP = std::variant<IPV4, IPV6>;

struct SocketClient : public std::enable_shared_from_this<SocketClient> {
 public:
  using ptr = std::shared_ptr<SocketClient>;

  using data_callback =
    std::function<void(bee::OrError<bee::DataBuffer>&& buf)>;

  SocketClient(const SocketClient&) = delete;
  SocketClient(SocketClient&&) = default;

  ~SocketClient();

  static bee::OrError<ptr> connect(const IP& ip, int port);

  bee::OrError<bee::Unit> send(std::string&& data);
  bee::OrError<bee::Unit> send(bee::DataBuffer&& data);

  static bee::OrError<IP> resolve_host(const std::string& hostname);

  const AsyncFD::ptr& fd() const;

  bool close();

  bool is_closed() const;

  Task<bee::Unit> closed();

  static ptr of_fd(AsyncFD::ptr&& fd);

  void set_data_callback(data_callback&& data_callback);

  Deferred<bee::OrError<bee::Unit>> flushed();

 private:
  explicit SocketClient(AsyncFD::ptr&& fd);

  void _on_ready();

  void _call_data_callback(bee::OrError<bee::DataBuffer>&& buf);

  AsyncFD::ptr _fd;

  data_callback _data_callback;

  bool _is_in_data_callback = false;

  bool _closed = false;
};

struct SocketServer {
 public:
  using ptr = std::shared_ptr<SocketServer>;

  using connection_callback =
    std::function<Task<bee::Unit>(bee::OrError<SocketClient::ptr>&&)>;

  ~SocketServer();

  explicit SocketServer(SocketServer&&) = default;
  SocketServer& operator=(SocketServer&&) = delete;

  static bee::OrError<ptr> listen(
    std::optional<int> port, connection_callback&& connection_callback);

  bee::OrError<std::optional<SocketClient::ptr>> accept();

  bee::OrError<int> port() const;

  const AsyncFD::ptr& fd() const;

  bool close();

 private:
  explicit SocketServer(
    AsyncFD::ptr&& fd, connection_callback&& connection_callback);

  void _on_ready();

  AsyncFD::ptr _fd;

  connection_callback _connection_callback;
};

} // namespace async
