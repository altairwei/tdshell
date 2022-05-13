#include "utils.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>
#include <cstring>

namespace StrUtil {

const std::string WHITESPACE = " \n\r\t\f\v";

std::string ltrim(const std::string &s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string &s)
{
  return rtrim(ltrim(s));
}

const std::string ELLIPSIS("...");
std::string elidedText(const std::string& text, int width, ElideMode mode) {
  if (text.length() <= width)
      return text;

  auto textLen = text.length();
  if (mode == ElideMode::Left) {
    return ELLIPSIS + text.substr(textLen - width, width);
  } else if (mode == ElideMode::Right) {
    return text.substr(0, width - ELLIPSIS.length()) + ELLIPSIS;
  } else {
    float partlen = (width - ELLIPSIS.length()) / 2.0;
    auto before = text.substr(0, std::ceil(partlen));
    auto after = text.substr(textLen - std::floor(partlen), std::floor(partlen));
    return before + ELLIPSIS + after;
  }

}

std::string join(std::vector<std::string> const &strings, std::string delim)
{
    std::stringstream ss;
    std::copy(strings.begin(), strings.end(),
        std::ostream_iterator<std::string>(ss, delim.c_str()));
    return ss.str();
}

std::vector<std::string> split(const std::string &str, const std::string &sep)
{
  char* cstr = const_cast<char*>(str.c_str());
  char* current;
  std::vector<std::string> arr;
  current = strtok(cstr, sep.c_str());
  while(current != NULL) {
    arr.push_back(current);
    current = strtok(NULL,sep.c_str());
  }

  return arr;
}

} // StrUtil

namespace PrintUtil
{

void printMessage(std::ostream& out, MessagePtr &msg) {
  out << "[msg_id: " << msg->id_ << "] ";
  td_api::downcast_call(
      *(msg->content_), overloaded(
        [&out](td_api::messageText &content) {
          out << "[type: Text] [text: "
              << content.text_->text_ << "]" << std::endl;
        },
        [&out](td_api::messageVideo &content) {
          out << "[type: Video] [caption: "
              << content.caption_->text_ << "] "
              << "[video: " << content.video_->file_name_ << "]"
              << std::endl;
        },
        [&out](td_api::messageDocument &content) {
          out << "[type: Document] [text: "
              << content.document_->file_name_ << "]" << std::endl;
        },
        [&out](td_api::messagePhoto &content) {
          out << "[type: Photo] [caption: "
              << content.caption_->text_ << "]" << std::endl;
        },
        [&out](td_api::messagePinMessage &content) {
          out << "[type: Pin] [ pinned message: "
              << content.message_id_ << "]" << std::endl;
        },
        [&out, &msg](td_api::messageChatJoinByLink &content) {
          out << "[type: Join] [ A new member joined the chat via an invite link. ]"
              << std::endl;
        },
        [&out, &msg](td_api::messageChatJoinByRequest &content) {
          out << "[type: Join] [ A new member was accepted to the chat by an administrator. ]"
              << std::endl;
        },
        [&out](auto &content) {
          out << "[text: Unsupported" << "]" << std::endl;
        }
      )
  );

}

void printProgress(std::ostream& out, std::string filename, int32_t total, int32_t downloaded) {
  double progress = double(downloaded) / double(total);

  int barWidth = 50;
  out << StrUtil::elidedText(filename, 20, StrUtil::Middle);
  out << " [";
  int pos = barWidth * progress;
  for (int i = 0; i < barWidth; ++i) {
      if (i < pos) out << "=";
      else if (i == pos) out << ">";
      else out << " ";
  }
  out << "] " << int(progress * 100.0) << " %\r";

  out.flush();
}

} // PrintUtil