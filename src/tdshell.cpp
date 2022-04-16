#include "tdshell.h"

#include <cli/detail/rang.h>
#include <termcolor/termcolor.hpp>

TdShell::TdShell() {
  core_ = std::make_unique<TdCore>();
}

void TdShell::open() {
  core_->start();
  core_->waitLogin();
}

void TdShell::close() {
  core_->stop();
}

void TdShell::cmdChats(std::ostream& out) {
  std::promise<ChatsPtr> prom;
  core_->getChats(prom, 100);
  auto chats = prom.get_future().get();

  for (auto chat_id : chats->chat_ids_) {
    std::promise<ChatPtr> chat_prom;
    core_->getChat(chat_prom, chat_id);
    auto chat = chat_prom.get_future().get();

    td_api::downcast_call(
      *(chat->type_), overloaded(
        [&out](td_api::chatTypeSupergroup &type) {
          if (type.is_channel_) {
            out << "📢 ";
          } else {
            out << "👥 ";
          }
        },
        [&out](td_api::chatTypePrivate &type) {
          out << "👤 ";
        },
        [&out](td_api::chatTypeSecret &type) {
          out << "👤 ";
        },
        [&out](auto &type) {
          out << "👥 ";
        }
      )
    );

    out << "[chat_id: " << chat_id << "] ";
    out << chat->title_;
    out << std::endl;
  }
}

void TdShell::cmdDownload(
    std::ostream& out,
    int64_t chat_id,
    std::vector<int32_t> &message_ids) {

  core_->downloadFiles(chat_id, message_ids);

}

void TdShell::cmdHistory(std::ostream& out, std::string chat_title, uint limit)
{
  auto id = core_->get_chat_id(chat_title);
  if (id == 0)
    throw std::logic_error("Not found chat " + chat_title);

  cmdHistory(out, id, limit);
}

void TdShell::cmdHistory(std::ostream& out, int64_t chat_id, uint limit) {
  std::promise<MessagesPtr> prom;
  auto fut = prom.get_future();
  core_->getChatHistory(prom, chat_id, limit == 0 ? 50 : limit);
  auto messages = fut.get();
  for (auto &msg : messages->messages_) {
    std::lock_guard<std::mutex> guard{output_lock};

    out << "[msg_id: " << msg->id_ << "] ";
    td_api::downcast_call(
        *(msg->content_), overloaded(
          [&out, this](td_api::messageText &content) {
            out << "[type: Text] [text: "
                << content.text_->text_ << "]" << std::endl;
          },
          [&out, this](td_api::messageVideo &content) {
            out << "[type: Video] [caption: "
                << content.caption_->text_ << "] "
                << "[video: " << content.video_->file_name_ << "]"
                << std::endl;
          },
          [&out, this](td_api::messageDocument &content) {
            out << "[type: Document] [text: "
                << content.document_->file_name_ << "]" << std::endl;
          },
          [&out, this](td_api::messagePhoto &content) {
            out << "[type: Photo] [text: "
                << content.caption_->text_ << "]" << std::endl;
          },
          [&out, this](td_api::messagePinMessage &content){
            out << "[type: Pin] [ pinned message: "
                << content.message_id_ << "]" << std::endl;
          },
          [&out](auto &content) {
            out << "[text: Unsupported" << "]" << std::endl;
          }
        )
    );
  }
}

void TdShell::error(std::ostream& out, std::string msg) {
  out << termcolor::red << msg << termcolor::reset << std::endl;
}