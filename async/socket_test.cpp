#include <thread>

#include "deferred_awaitable.hpp"
#include "socket.hpp"
#include "testing.hpp"

#include "bee/util.hpp"

using namespace async;

using std::nullopt;

namespace async {
namespace {

ASYNC_TEST(basic)
{
  must(
    server,
    SocketServer::listen(
      nullopt, [](bee::OrError<SocketClient::ptr>&& sock_or_error) -> Task<> {
        must(sock, sock_or_error);
        sock->send("hello");
        co_await sock->flushed();
        sock->close();
      }));

  must(port, server->port());

  must(ip, SocketClient::resolve_host("localhost"));
  must(client, SocketClient::connect(ip, port));

  auto done = Ivar<>::create();

  client->set_data_callback(
    [done](bee::OrError<bee::DataBuffer>&& buf_or_error) {
      must(buf, buf_or_error);
      if (buf.empty()) {
        P("Got eof");
        done->fill();
      } else {
        P("Got data: '$'", buf);
      }
    });

  co_await done;

  server->close();
  client->close();
}

ASYNC_TEST(send_data_buffer)
{
  must(
    server,
    SocketServer::listen(
      nullopt, [](bee::OrError<SocketClient::ptr>&& sock_or_error) -> Task<> {
        must(sock, sock_or_error);
        {
          bee::DataBuffer buffer;
          buffer.write("hello");
          buffer.write("world");
          buffer.write("end");
          must_unit(sock->send(std::move(buffer)));
        }
        co_await sock->flushed();
        sock->close();
      }));

  must(port, server->port());

  must(ip, SocketClient::resolve_host("localhost"));
  must(client, SocketClient::connect(ip, port));

  auto done = Ivar<>::create();

  client->set_data_callback(
    [done](bee::OrError<bee::DataBuffer>&& buf_or_error) {
      must(buf, buf_or_error);
      if (buf.empty()) {
        P("Got eof");
        done->fill();
      } else {
        P("Got data: '$'", buf);
      }
    });

  co_await done;

  server->close();
  client->close();
}

} // namespace
} // namespace async
