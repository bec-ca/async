#pragma once

#include <concepts>
#include <thread>

#include "once.hpp"
#include "pipe.hpp"
#include "queue_bridge.hpp"

#include "bee/queue.hpp"

namespace async {

template <std::move_constructible In, std::move_constructible Out>
struct OutThread {
 public:
  struct QueuePair {
    bee::Queue<In> input_queue;
    QueueBridge<Out> output_queue;

    void send(Out&& value) { output_queue.push(std::move(value)); }

    QueuePair(bee::Pipe&& data_pipe) : output_queue(std::move(data_pipe)) {}

    QueuePair(const QueuePair& other) = delete;
    QueuePair(QueuePair&& other) = default;
  };

  using ptr = std::shared_ptr<OutThread>;
  using ThreadFunction = std::function<void(QueuePair& queue_pair)>;

  OutThread(const OutThread& other) = delete;
  OutThread(OutThread&& other) = delete;

  OutThread(ThreadFunction&& fn, bee::Pipe&& data_pipe, int num_threads)
      : _queue_pair(std::move(data_pipe))
  {
    if (num_threads > 1) {
      std::vector<std::thread> worker_threads;
      for (int i = 0; i < num_threads; i++) {
        worker_threads.emplace_back([this, fn]() { fn(_queue_pair); });
      }
      _waiter_thread.emplace(
        [this, worker_threads = std::move(worker_threads)]() mutable {
          for (auto& t : worker_threads) { t.join(); }
          worker_threads.clear();
          _queue_pair.output_queue.close_write();
        });
    } else {
      _waiter_thread.emplace([this, fn = std::move(fn)]() mutable {
        fn(_queue_pair);
        _queue_pair.output_queue.close_write();
      });
    }
  }

  ~OutThread() { close(); }

  void send(In&& value) { _queue_pair.input_queue.push(std::move(value)); }

  const typename Pipe<Out>::ptr& output_pipe()
  {
    return _queue_pair.output_queue.pipe();
  }

  Task<std::optional<Out>> receive() { return output_pipe()->next_value(); }

  static bee::OrError<ptr> create(ThreadFunction&& fn, int num_threads)
  {
    bail(data_pipe, bee::Pipe::create());
    return std::make_shared<OutThread>(
      std::move(fn), std::move(data_pipe), num_threads);
  }

  void close()
  {
    if (_closed) return;
    _closed = true;
    _queue_pair.input_queue.close();
    _maybe_join();
    _queue_pair.output_queue.close();
  }

 private:
  void _maybe_join()
  {
    if (_waiter_thread.has_value()) {
      _waiter_thread->join();
      _waiter_thread = std::nullopt;
    }
  }
  QueuePair _queue_pair;

  std::optional<std::thread> _waiter_thread;

  std::queue<Out> _output_queue;

  bool _closed = false;
};

} // namespace async
