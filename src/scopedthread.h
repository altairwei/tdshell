#ifndef SCOPED_THREAD_H
#define SCOPED_THREAD_H

#include <thread>

class ScopedThread {
public:
  template <typename... Arg>
  ScopedThread(Arg&&... arg)
    : thread_(
        std::forward<Arg>(arg)...)
  {}
  ScopedThread(
    ScopedThread&& other)
    : thread_(
        std::move(other.thread_))
  {}
  ScopedThread(
    const ScopedThread&) = delete;
  ~ScopedThread()
  {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

private:
  std::thread thread_;
};

#endif // SCOPED_THREAD_H