#ifndef COMMON_H
#define COMMON_H

#include <mutex>
#include <iostream>

#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

extern std::mutex output_lock;

namespace td_api = td::td_api;

typedef td::tl_object_ptr<td_api::chats> ChatListPtr;
typedef td::tl_object_ptr<td_api::chat> ChatPtr;
typedef td::tl_object_ptr<td_api::messages> MessageListPtr;
typedef td::tl_object_ptr<td_api::message> MessagePtr;
typedef td::tl_object_ptr<td_api::file> FilePtr;
typedef td_api::object_ptr<td_api::Object> ObjectPtr;
typedef td_api::object_ptr<td_api::filePart> FilePartPtr;

// overloaded
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , public overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

static void printMessage(std::ostream& out, MessagePtr &msg) {
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

static void printProgress(std::ostream& out, std::string filename, int32_t total, int32_t downloaded) {
  double progress = double(downloaded) / double(total);

  int barWidth = 50;
  out << filename;
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

#endif // COMMMON_H