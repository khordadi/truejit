#pragma once

#include <Globals.h>
#include <bits/ranges_algo.h>
#include <condition_variable>
#include <fmt/color.h>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>
#include <tuple>
#include <vector>

// #define TRACE_THREAD_POOL
#define PROFILE_THREAD_POOL

#ifdef TRACE_THREAD_POOL
#include "Logging.h"
static Logger L{"thread-pool", fmt::color::gold};
static auto EnqueueSS = SubScope{"enqueue", fmt::color::cyan};
static auto StartSS = SubScope{"start", fmt::color::light_green};
static auto EndSS = SubScope{"end", fmt::color::red};
#endif

enum Priority { LOW = 0, HIGH = 1 };

class ThreadPool {
public:
  using NumberOfThreads = std::optional<u32>;
  explicit ThreadPool(const NumberOfThreads Num = std::nullopt) {
    const auto Threads = Num.value_or(std::thread::hardware_concurrency());
    for (auto i = 0u; i < Threads; ++i)
      workers.emplace_back([this] {
        for (;;) {
          std::function<void()> task;
          std::string PriorityStr;
          {
            std::unique_lock lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty())
              return;

            task = tasks.front();
            tasks.pop();
          }
          task();
        }
      });
  }

  template <class F, class... Args>
  std::future<std::result_of_t<F(Args...)>> enqueue(F &&f, Args &&...args) {
    using return_type = std::result_of_t<F(Args...)>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
      std::unique_lock lock(queue_mutex);

      // don't allow enqueueing after stopping the pool
      if (stop)
        throw std::runtime_error("enqueue on stopped ThreadPool");

      tasks.push([task] { (*task)(); });
    }
    condition.notify_one();
    return res;
  }

  ~ThreadPool() {
    {
      std::unique_lock lock(queue_mutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
      worker.join();
  }

private:
  // need to keep track of threads so we can join them
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  // synchronization
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop{};
};

inline std::string GetName(const ThreadPool::NumberOfThreads Num) {
  return Num.has_value() ? std::to_string(Num.value()) : "multi";
}
