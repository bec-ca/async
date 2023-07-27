#pragma once

#include "async_fd.hpp"
#include "pipe.hpp"
#include "task.hpp"

#include "bee/fd.hpp"
#include "bee/queue.hpp"

namespace async {

template <class T> struct ThreadBridgeReader {
 public:
  using queue_t = std::shared_ptr<bee::Queue<T>>;
  using ptr = std::shared_ptr<ThreadBridgeReader>;

  ThreadBridgeReader(const AsyncFD::ptr& read_fd, const queue_t& queue)
      : _read_fd(read_fd), _queue(queue)
  {
    async::schedule_task(
      [queue = _queue, fd = _read_fd, pipe = _pipe]() -> Task<> {
        while (true) {
          bee::DataBuffer buf;
          must(res, co_await fd->read_async(buf));
          if (pipe->is_closed()) { break; }
          while (auto v = queue->pop_non_blocking()) {
            pipe->push(std::move(*v));
          }
          if (res.is_eof() || queue->is_closed_and_empty()) { break; }
        }
        queue->close();
        pipe->close();
      });
  }

  ~ThreadBridgeReader() { close(); }

  static bee::OrError<ptr> create()
  {
    bail(fd_pipe, bee::Pipe::create());

    bail(read_fd, AsyncFD::of_fd(fd_pipe.read_fd, false));

    return ptr(new ThreadBridgeReader(read_fd, fd_pipe.write_fd));
  }

  ThreadBridgeReader(const ThreadBridgeReader& other) = delete;
  ThreadBridgeReader(ThreadBridgeReader&& other) = delete;

  void close() { _pipe->close(); }

  Task<std::optional<T>> pop() { return _pipe->next_value(); }

  const typename Pipe<T>::ptr& pipe() { return _pipe; }

 private:
  AsyncFD::ptr _read_fd;

  queue_t _queue;
  typename Pipe<T>::ptr _pipe = Pipe<T>::create();

  bool _closed = false;
};

template <class T> struct ThreadBridgeWriter {
 public:
  using queue_t = std::shared_ptr<bee::Queue<T>>;
  using ptr = std::shared_ptr<ThreadBridgeWriter>;

  ThreadBridgeWriter(const bee::FD::shared_ptr& write_fd, const queue_t& queue)
      : _write_fd(write_fd), _queue(queue)
  {}

  ~ThreadBridgeWriter() { close(); }

  static bee::OrError<ptr> create()
  {
    bail(fd_pipe, bee::Pipe::create());

    bail(read_fd, AsyncFD::of_fd(fd_pipe.read_fd, false));

    return ptr(new ThreadBridgeWriter(read_fd, fd_pipe.write_fd));
  }

  ThreadBridgeWriter(const ThreadBridgeWriter& other) = delete;
  ThreadBridgeWriter(ThreadBridgeWriter&& other) = delete;

  template <std::convertible_to<T> U> void push(U&& item)
  {
    _queue->push(std::forward<U>(item));
    _notify();
  }

  void close()
  {
    if (_closed) { return; }
    _queue->close();
    _notify();
    _closed = true;
  }

  bool is_closed() const { return _closed; }

 private:
  void _notify() { _write_fd->write(" "); }

  bee::FD::shared_ptr _write_fd;
  queue_t _queue;

  bool _closed = false;
};

template <class T>
bee::OrError<std::pair<
  typename ThreadBridgeReader<T>::ptr,
  typename ThreadBridgeWriter<T>::ptr>>
create_thread_bridge_pair()
{
  bail(fd_pipe, bee::Pipe::create());
  bail(read_fd, AsyncFD::of_fd(fd_pipe.read_fd, false));

  auto queue = std::make_shared<bee::Queue<T>>();

  auto reader = std::make_shared<ThreadBridgeReader<T>>(read_fd, queue);
  auto writer =
    std::make_shared<ThreadBridgeWriter<T>>(fd_pipe.write_fd, queue);

  return make_pair(reader, writer);
}

} // namespace async
