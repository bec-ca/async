#include "scheduler_test_common.hpp"

#include "deferred_awaitable.hpp"
#include "run_scheduler.hpp"
#include "socket.hpp"

#include "bee/data_buffer.hpp"

using bee::DataBuffer;

using std::make_shared;
using std::nullopt;
using std::string;

namespace async {
namespace test {
namespace {

Task<> basic_test_impl()
{
  auto listen_callback =
    [](bee::OrError<SocketClient::ptr>&& sock_or_err) -> Task<> {
    P("Incoming server connection");
    must(sock, sock_or_err);
    auto data_callback = [=](const bee::OrError<DataBuffer>& buf_or_err) {
      must(buf, buf_or_err);
      if (!buf.empty()) {
        P("Incoming data from client: $", buf);
        sock->close();
      }
    };
    sock->set_data_callback(std::move(data_callback));
    must_unit(sock->send("hello there"));
    P("data sent to client");
    co_await sock->closed();
  };

  auto server_or_err =
    SocketServer::listen(nullopt, std::move(listen_callback));

  must(server, server_or_err);

  must(port, server->port());

  must(ip, SocketClient::resolve_host("localhost"));
  must(client, SocketClient::connect(ip, port));

  client->set_data_callback([=](const bee::OrError<DataBuffer>& buf_or_err) {
    must(buf, buf_or_err);
    if (buf.empty()) {
      P("got eof");
      client->close();
    } else {
      P("Incoming data from server: $", buf);
      DataBuffer data_to_send_server("hello server, this is client");
      client->send(std::move(data_to_send_server));
    }
  });

  co_await client->closed();

  client->close();
  server->close();
}

Task<> large_data_impl()
{
  string large_data;
  for (int i = 0; i < 1000000; i++) { large_data += "hello_world\n"; }

  must(
    server,
    SocketServer::listen(
      nullopt,
      [large_data](
        const bee::OrError<SocketClient::ptr>& sock_or_err) -> Task<> {
        P("Incoming server connection");
        must(sock, sock_or_err);
        must_unit(sock->send(bee::copy(large_data)));
        co_await sock->flushed();
        sock->close();
      }));

  must(port, server->port());

  must(ip, SocketClient::resolve_host("localhost"));
  must(client, SocketClient::connect(ip, port));

  auto recv_count = make_shared<int>(0);
  auto buffer = make_shared<DataBuffer>();

  client->set_data_callback([=](bee::OrError<DataBuffer>&& buf_or_err) {
    (*recv_count)++;
    must(buf, buf_or_err);
    if (buf.empty()) {
      P("got eof");
    } else {
      buffer->write(std::move(buf));
    }
  });

  co_await client->closed();

  P("bytes received: $  recv_count>2: $", buffer->size(), *recv_count > 2);

  server->close();
  client->close();
}

} // namespace

void SchedulerTestCommon::basic_test()
{
  must(ctx, create_context());
  RunScheduler::run(basic_test_impl, std::move(ctx));
}

void SchedulerTestCommon::large_data()
{
  must(ctx, create_context());
  RunScheduler::run(large_data_impl, std::move(ctx));
}

} // namespace test
} // namespace async
