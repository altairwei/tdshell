#include "utils.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>
#include <cstring>
#include <iostream>
#include <string>
#include <csignal>
#include <iomanip>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

#include <td/utils/utf8.h>

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
std::string elidedText(const std::string& input, std::uint8_t width, StrLoc mode, bool rmLinebreak) {
  std::string text = input;

  if (rmLinebreak) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\r', ' ');
  }

  size_t textLen = td::utf8_length(text);

  if (textLen <= width)
      return text;

  if (mode == StrLoc::Left) {
    return ELLIPSIS + td::utf8_substr(text, textLen - width, width);
  } else if (mode == StrLoc::Right) {
    return td::utf8_substr(text, 0, width - ELLIPSIS.length()) + ELLIPSIS;
  } else {
    float partlen = (width - ELLIPSIS.length()) / 2.0;
    auto before = td::utf8_substr(text, 0, std::ceil(partlen));
    auto after = td::utf8_substr(text, textLen - std::floor(partlen), std::floor(partlen));
    return before + ELLIPSIS + after;
  }
}

std::string paddingText(const std::string& text, std::uint8_t width, char pad_char, StrLoc location)
{
  size_t textLen = td::utf8_length(text);
  if (textLen >= width) {
      return text;
  }

  size_t pad_size = width - textLen;
  if (location == Left) {
      return std::string(pad_size, pad_char) + text;
  } else {
      return text + std::string(pad_size, pad_char);
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

namespace ConsoleUtil
{

std::mutex output_lock;

void printMessage(std::ostream& out, MessagePtr &msg, bool elided, std::uint8_t elideWidth) {
  out << "[msg_id: " << msg->id_ << "] ";
  td_api::downcast_call(
      *(msg->content_), overloaded(
        [&](td_api::messageText &content) {
          out << "[type: Text] [text: "
              << (elided ? StrUtil::elidedText(content.text_->text_, elideWidth, StrUtil::Right)
                                : content.text_->text_) << "]"
              << std::endl;
        },
        [&](td_api::messageVideo &content) {
          out << "[type: Video] [caption: "
              << (elided ? StrUtil::elidedText(content.caption_->text_, elideWidth, StrUtil::Right)
                                : content.caption_->text_) << "] "
              << "[video: "
              << (elided ? StrUtil::elidedText(content.video_->file_name_, 20, StrUtil::Middle)
                                : content.video_->file_name_) << "]"
              << std::endl;
        },
        [&](td_api::messageDocument &content) {
          out << "[type: Document] [text: "
              << (elided ? StrUtil::elidedText(content.document_->file_name_, 20, StrUtil::Middle)
                                : content.document_->file_name_) << "]" << std::endl;
        },
        [&](td_api::messagePhoto &content) {
          out << "[type: Photo] [caption: "
              << (elided ? StrUtil::elidedText(content.caption_->text_, elideWidth, StrUtil::Right)
                                : content.caption_->text_) << "]" << std::endl;
        },
        [&](td_api::messagePinMessage &content) {
          out << "[type: Pin] [ pinned message: "
              << content.message_id_ << "]" << std::endl;
        },
        [&](td_api::messageChatJoinByLink &content) {
          out << "[type: Join] [ A new member joined the chat via an invite link. ]"
              << std::endl;
        },
        [&](td_api::messageChatJoinByRequest &content) {
          out << "[type: Join] [ A new member was accepted to the chat by an administrator. ]"
              << std::endl;
        },
        [&](auto &content) {
          out << "[text: Unsupported]" << std::endl;
        }
      )
  );

}

void printProgress(std::ostream& out, std::string filename, int32_t total, int32_t downloaded) {
  double progress = double(downloaded) / double(total);

  // Print percentage
  out << std::setw(3) << std::setfill(' ') << int(progress * 100.0) << " %";

  // Print progress bar
  int barWidth = 50;
  out << " [";
  int pos = int(barWidth * progress);
  for (int i = 0; i < barWidth; ++i) {
      if (i < pos) out << "=";
      else if (i == pos) out << ">";
      else out << " ";
  }
  out << "] ";

  // Print filename
  out << StrUtil::paddingText(StrUtil::elidedText(filename, 20, StrUtil::Middle), 20, ' ', StrUtil::Right)
      << "\r";

  out.flush();
}

std::string getPassword(const std::string& prompt = "Enter password: ") {
    std::string password;

#ifdef _WIN32
    std::cout << prompt;
    char ch = 0;
    while ((ch = _getch()) != '\r') {
        if(ch == '\0' || ch == 0xE0) { // Ignore function keys
            _getch();
            continue;
        }
        if(ch == '\b' && password.length() != 0) {
            std::cout << "\b \b";  // Erase the last character in the console as well.
            password.pop_back();
        }
        else if (ch != '\b') {
            std::cout << '*';
            password.push_back(ch);
        }
    }
    std::cout << std::endl;
#else
    std::cout << prompt;
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    getline(std::cin, password);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;
#endif

    return password;
}

} // PrintUtil