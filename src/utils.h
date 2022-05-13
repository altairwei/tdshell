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
std::string elidedText(const std::string& text, int width, ElideMode mode);

std::string join(std::vector<std::string> const &strings, std::string delim);
std::vector<std::string> split(const std::string &str, const std::string &sep);

} // namespace StrUtil

namespace PrintUtil
{

void printMessage(std::ostream& out, MessagePtr &msg);
void printProgress(std::ostream& out, std::string filename, int32_t total, int32_t downloaded);

} // PrintUtil

#endif // SRC_UTILS_H