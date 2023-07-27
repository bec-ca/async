#pragma once

#include <mutex>

#include "async.hpp"
#include "pipe.hpp"

namespace async {

template <class T> struct QueueBridge {
 public:
  QueueBridge(bee::Pipe&& data_pipe) : _data_pipe(std::move(data_pipe))
  {
    _data_pipe.read_fd->set_blocking(false);
    add_fd(_data_pipe.read_fd, [this]() {
      bee::DataBuffer buf;
      auto res = _data_pipe.read_fd->read_all_available(buf);
      _flush();
      if (res.is_error() || res.value().is_eof()) { close(); }
    });
  }

  QueueBridge(const QueueBridge& other) = delete;
  QueueBridge(QueueBridge&& other) = delete;

  ~QueueBridge() { close(); }

  void push(T&& item)
  {
    auto lock = std::unique_lock(_mutex);
    _local_queue.push(std::move(item));
    _data_pipe.write_fd->write("r");
  }

  void close()
  {
    if (_closed) { return; }
    _closed = true;
    _flush();
    _pipe->close();
    remove_fd(_data_pipe.read_fd);
    _data_pipe.close();
  }

  void close_write() { _data_pipe.write_fd->close(); }

  const typename Pipe<T>::ptr& pipe() { return _pipe; }

 private:
  void _flush()
  {
    auto lk = std::unique_lock(_mutex);
    while (!_local_queue.empty()) {
      _pipe->push(std::move(_local_queue.front()));
      _local_queue.pop();
    }
  }

  std::mutex _mutex;
  std::queue<T> _local_queue;
  bee::Pipe _data_pipe;

  typename Pipe<T>::ptr _pipe = Pipe<T>::create();

  bool _closed = false;
};

} // namespace async
