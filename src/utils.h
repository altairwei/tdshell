#ifndef SRC_UTILS_H
#define SRC_UTILS_H

#include <string>
#include <future>
#include "common.h"

namespace StrUtil
{

std::string ltrim(const std::string &s);
std::string rtrim(const std::string &s);
std::string trim(const std::string &s);

enum StrLoc { Left, Middle, Right };
std::string elidedText(const std::string& text, std::uint8_t width = 20,
                       StrLoc mode = Right, bool rmLinebreak = true);
std::string paddingText(const std::string& text, std::uint8_t width,
                        char pad_char = ' ', StrLoc location = Left);

std::string join(std::vector<std::string> const &strings, std::string delim);
std::vector<std::string> split(const std::string &str, const std::string &sep);

} // namespace StrUtil

namespace ConsoleUtil
{

void printMessage(std::ostream& out, MessagePtr &msg, bool elided = true, std::uint8_t elideWidth = 20);
void printProgress(std::ostream& out, std::string filename, int32_t total, int32_t downloaded);
std::string getPassword(const std::string& prompt);

} // namespace ConsoleUtil

namespace AsynUtil
{

// Processes a vector of futures by executing a user-provided function on each
// future once it's ready, and an optional function once all futures are ready.
template<typename T>
void waitFutures(std::vector<std::future<T>>& futures,
                     std::function<void(T, std::size_t)> on_each,
                     std::function<void()> on_all = []() {}) {
  std::vector<bool> completed(futures.size(), false);
  std::size_t completed_count = 0;
  const std::size_t total = futures.size();

  while (completed_count < total) {
    for (std::size_t i = 0; i < total; ++i) {
      if (!completed[i] && futures[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        T value = futures[i].get();
        on_each(std::move(value), i);
        completed[i] = true;
        ++completed_count;
      }
    }
  }

  on_all();
}

} // namespace AsynUtil

#endif // SRC_UTILS_H