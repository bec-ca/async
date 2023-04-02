#include "socket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "bee/file_descriptor.hpp"
#include "bee/util.hpp"

using bee::FileDescriptor;
using std::decay_t;
using std::is_same_v;
using std::nullopt;
using std::optional;
using std::string;
using std::weak_ptr;

namespace async {

#ifdef __APPLE__

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#endif

namespace {

bee::OrError<FileDescriptor::shared_ptr> create_socket_fd(int family)
{
  int fd = socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd == -1) { shot("Failed to create socket: $", strerror(errno)); }
  return FileDescriptor(fd).to_shared();
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// SocketServer
//

SocketServer::SocketServer(
  AsyncFD::ptr&& fd, connection_callback&& connection_callback)
    : _fd(std::move(fd)), _connection_callback(std::move(connection_callback))
{}

SocketServer::~SocketServer() {}

const AsyncFD::ptr& SocketServer::fd() const { return _fd; }

bee::OrError<SocketServer::ptr> SocketServer::listen(
  optional<int> port, connection_callback&& connection_callback)
{
  int family = AF_INET6;
  bail(fd, create_socket_fd(family));

  {
    int value = 1;
    if (
      setsockopt(
        fd->int_fd(), SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
      shot("Failed to set socket option: $", strerror(errno));
    }
  }

  int port_number = port.value_or(0);

  sockaddr_in6 address;
  memset(&address, 0, sizeof(address));
  address.sin6_family = family;
  address.sin6_port = htons(port_number);

  if (::bind(fd->int_fd(), (sockaddr*)&address, sizeof(address)) < 0) {
    shot("Failed to bind to port $: $", port_number, strerror(errno));
  }

  if (::listen(fd->int_fd(), 16) < 0) {
    shot("Failed to listen to port: $", strerror(errno));
  }

  bail_unit(fd->set_blocking(false));

  bail(afd, AsyncFD::of_fd(bee::copy(fd), true));

  auto sock =
    ptr(new SocketServer(bee::copy(afd), std::move(connection_callback)));

  afd->set_ready_callback([weak = weak_ptr(sock)]() {
    if (auto ptr = weak.lock()) { ptr->_on_ready(); }
  });

  return sock;
}

bee::OrError<int> SocketServer::port() const
{
  sockaddr_in address;
  memset(&address, 0, sizeof(address));
  socklen_t len = sizeof(address);
  if (getsockname(_fd->int_fd(), (sockaddr*)&address, &len) == -1) {
    shot("Failed to get port number: $", strerror(errno));
  }

  return ntohs(address.sin_port);
}

bee::OrError<optional<SocketClient::ptr>> SocketServer::accept()
{
  bail(client_fd_opt, _fd->accept());

  if (!client_fd_opt.has_value()) { return nullopt; }
  auto& client_fd = *client_fd_opt;

  return SocketClient::of_fd(std::move(client_fd));
}

void SocketServer::_on_ready()
{
  if (_connection_callback == nullptr) { return; }
  while (true) {
    auto client_fd_opt_or_error = _fd->accept();
    if (client_fd_opt_or_error.is_error()) {
      schedule_task(_connection_callback, client_fd_opt_or_error.error());
      break;
    }
    auto& client_fd_opt = client_fd_opt_or_error.value();

    if (!client_fd_opt.has_value()) { break; }
    auto& client_fd = *client_fd_opt;

    schedule_task(
      _connection_callback, SocketClient::of_fd(std::move(client_fd)));
  }
}

bool SocketServer::close()
{
  if (_fd == nullptr) return false;
  auto ret = _fd->close();
  _fd = nullptr;
  _connection_callback = nullptr;
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// SocketClient
//

SocketClient::~SocketClient() {}

SocketClient::SocketClient(AsyncFD::ptr&& fd) : _fd(std::move(fd)) {}

const AsyncFD::ptr& SocketClient::fd() const { return _fd; }

bee::OrError<SocketClient::ptr> SocketClient::connect(const IP& ip, int port)
{
  sockaddr_in serv_addr_v4;
  sockaddr_in6 serv_addr_v6;

  size_t addr_len;

  auto serv_addr = visit(
    [&](const auto& ip) {
      using T = decay_t<decltype(ip)>;
      if constexpr (is_same_v<T, IPV4>) {
        serv_addr_v4.sin_addr.s_addr = ip.address;
        serv_addr_v4.sin_family = AF_INET;
        serv_addr_v4.sin_port = htons(port);
        addr_len = sizeof(serv_addr_v4);
        return (sockaddr*)&serv_addr_v4;
      } else if constexpr (is_same_v<T, IPV6>) {
        serv_addr_v6.sin6_family = AF_INET6;
        serv_addr_v6.sin6_port = htons(port);
        memcpy(serv_addr_v6.sin6_addr.s6_addr, ip.address, sizeof(ip.address));
        addr_len = sizeof(serv_addr_v6);
        return (sockaddr*)&serv_addr_v6;
      } else {
        static_assert(bee::always_false_v<T> && "non exaustive visit");
      }
    },
    ip);

  bail(fd, create_socket_fd(serv_addr->sa_family));

  int ret = ::connect(fd->int_fd(), serv_addr, addr_len);
  if (ret != 0) {
    shot("Failed to connect to host (fd:$): $", fd->int_fd(), strerror(errno));
  }

  bail_unit(fd->set_blocking(false));

  bail(afd, AsyncFD::of_fd(std::move(fd), true));

  return of_fd(std::move(afd));
}

bee::OrError<bee::Unit> SocketClient::send(bee::DataBuffer&& data)
{
  return _fd->write(std::move(data));
}

bee::OrError<bee::Unit> SocketClient::send(string&& data)
{
  return _fd->write(std::move(data));
}

SocketClient::ptr SocketClient::of_fd(AsyncFD::ptr&& fd)
{
  auto sock = ptr(new SocketClient(bee::copy(fd)));
  fd->set_ready_callback([weak_ptr = weak_ptr(sock)]() {
    if (auto ptr = weak_ptr.lock()) { ptr->_on_ready(); }
  });
  return sock;
}

bee::OrError<IP> SocketClient::resolve_host(const string& hostname)
{
  addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
  hints.ai_protocol = 0;

  int ret = getaddrinfo(hostname.data(), nullptr, &hints, &servinfo);
  if (ret != 0) { shot("Failed to resolve host: $", gai_strerror(ret)); }

  optional<IP> output;
  for (auto p = servinfo; p != nullptr; p = p->ai_next) {
    if (p->ai_family == AF_INET6) {
      auto h = (sockaddr_in6*)servinfo->ai_addr;
      IPV6 ip;
      memcpy(ip.address, h->sin6_addr.s6_addr, sizeof(h->sin6_addr.s6_addr));
      output = ip;
      break;
    } else if (p->ai_family == AF_INET) {
      auto h = (sockaddr_in*)servinfo->ai_addr;
      output = IPV4{h->sin_addr.s_addr};
      break;
    }
  }
  freeaddrinfo(servinfo);

  if (!output.has_value()) {
    return bee::Error("No valid address for server found");
  }

  return *output;
}

bool SocketClient::is_closed() const { return _closed; }

bool SocketClient::close()
{
  if (is_closed()) { return false; }
  _closed = true;
  auto ret = _fd->close();
  if (!_is_in_data_callback) { _data_callback = nullptr; }
  return ret;
}

Task<bee::Unit> SocketClient::closed()
{
  if (_fd == nullptr) { return bee::unit; }
  return _fd->closed();
}

void SocketClient::_call_data_callback(bee::OrError<bee::DataBuffer>&& buf)
{
  if (is_closed()) { return; }
  assert(!_is_in_data_callback);
  _is_in_data_callback = true;
  _data_callback(std::move(buf));
  _is_in_data_callback = false;
  if (is_closed()) { _data_callback = nullptr; }
}

void SocketClient::_on_ready()
{
  if (is_closed()) { return; }
  bee::DataBuffer buf;
  auto result_or_error = _fd->read(buf);
  if (result_or_error.is_error()) {
    _call_data_callback(result_or_error.error());
    close();
    return;
  }

  auto& result = result_or_error.value();
  if (result.bytes_read() > 0) { _call_data_callback(std::move(buf)); }
  if (result.is_eof()) {
    _call_data_callback(bee::DataBuffer());
    close();
  }
}

void SocketClient::set_data_callback(data_callback&& data_callback)
{
  assert(_data_callback == nullptr && "Data callback already set");
  _data_callback = std::move(data_callback);
}

Deferred<bee::OrError<bee::Unit>> SocketClient::flushed()
{
  return _fd->flushed();
}

} // namespace async
