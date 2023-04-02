#include "socket.hpp"

#include "bee/util.hpp"
#include "deferred_awaitable.hpp"
#include "testing.hpp"

#include <thread>

using namespace async;
using bee::print_line;
using std::nullopt;

namespace async {
namespace {

CORO_TEST(basic)
{
  co_bail(
    server,
    SocketServer::listen(
      nullopt,
      [](bee::OrError<SocketClient::ptr>&& sock_or_error) -> Task<bee::Unit> {
        must(sock, sock_or_error);
        sock->send("hello");
        sock->flushed().iter(
          [sock](const bee::OrError<bee::Unit>&) { sock->close(); });
        return bee::unit;
      }));

  co_bail(port, server->port());

  co_bail(ip, SocketClient::resolve_host("localhost"));
  co_bail(client, SocketClient::connect(ip, port));

  auto done = Ivar<bee::Unit>::create();

  client->set_data_callback(
    [done](bee::OrError<bee::DataBuffer>&& buf_or_error) {
      must(buf, buf_or_error);
      if (buf.empty()) {
        print_line("Got eof");
        done->resolve(bee::unit);
      } else {
        print_line("Got data: '$'", buf.to_string());
      }
    });

  co_await done;

  server->close();
  client->close();

  co_return bee::unit;
}

CORO_TEST(send_data_buffer)
{
  co_bail(
    server,
    SocketServer::listen(
      nullopt,
      [](bee::OrError<SocketClient::ptr>&& sock_or_error) -> Task<bee::Unit> {
        must(sock, sock_or_error);
        {
          bee::DataBuffer buffer;
          buffer.write("hello");
          buffer.write("world");
          buffer.write("end");
          must_unit(sock->send(std::move(buffer)));
        }
        sock->flushed().iter(
          [sock](const bee::OrError<bee::Unit>&) { sock->close(); });
        return bee::unit;
      }));

  co_bail(port, server->port());

  co_bail(ip, SocketClient::resolve_host("localhost"));
  co_bail(client, SocketClient::connect(ip, port));

  auto done = Ivar<bee::Unit>::create();

  client->set_data_callback(
    [done](bee::OrError<bee::DataBuffer>&& buf_or_error) {
      must(buf, buf_or_error);
      if (buf.empty()) {
        print_line("Got eof");
        done->resolve(bee::unit);
      } else {
        print_line("Got data: '$'", buf.to_string());
      }
    });

  co_await done;

  server->close();
  client->close();

  co_return bee::unit;
}

} // namespace
} // namespace async
