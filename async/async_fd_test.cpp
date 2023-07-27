#include "async_fd.hpp"
#include "testing.hpp"

#include "bee/fd.hpp"

namespace async {
namespace {

ASYNC_TEST(read_async)
{
  must(pipe, bee::Pipe::create());

  must(read_fd, AsyncFD::of_fd(pipe.read_fd, false));
  must(write_fd, AsyncFD::of_fd(pipe.write_fd, false));

  P("Going to write");
  must_unit(write_fd->write("hello"));
  P("Waiting to flush");
  co_await write_fd->flushed();
  write_fd->close();

  P("Going to read");
  while (true) {
    bee::DataBuffer buf;
    must(result, co_await read_fd->read_async(buf));
    if (result.bytes_read() > 0) { P("Read: $", buf); }
    if (result.is_eof()) {
      P("Got EOF");
      break;
    }
  }
}

} // namespace
} // namespace async
