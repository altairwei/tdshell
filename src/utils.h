#ifndef SRC_UTILS_H
#define SRC_UTILS_H

#include <string>
#include "common.h"

namespace StrUtil
{

std::string ltrim(const std::string &s);
std::string rtrim(const std::string &s);
std::string trim(const std::string &s);

enum ElideMode { Left, Middle, Right };
std::string elidedText(const std::string& text, std::uint8_t width = 20,
                       ElideMode mode = Right, bool rmLinebreak = true);

std::string join(std::vector<std::string> const &strings, std::string delim);
std::vector<std::string> split(const std::string &str, const std::string &sep);

} // namespace StrUtil

namespace ConsoleUtil
{

void printMessage(std::ostream& out, MessagePtr &msg, bool elided = true, std::uint8_t elideWidth = 20);
void printProgress(std::ostream& out, std::string filename, int32_t total, int32_t downloaded);
std::string getPassword(const std::string& prompt);

} // ConsoleUtil

#endif // SRC_UTILS_H