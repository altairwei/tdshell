#include "tdshell.h"

#include <cli/detail/rang.h>
#include <termcolor/termcolor.hpp>
#include <td/telegram/td_api.hpp>

#include "common.h"

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
        [&out](td_api::chatTypeBasicGroup &type) {
          out << "🙌 ";
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
    std::vector<int64_t> &message_ids) {

  try {
    core_->downloadFiles(chat_id, message_ids);
  } catch (const std::exception& e) {
    error(out, e.what());
  }

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
            out << "[type: Photo] [caption: "
                << content.caption_->text_ << "]" << std::endl;
          },
          [&out, this](td_api::messagePinMessage &content) {
            out << "[type: Pin] [ pinned message: "
                << content.message_id_ << "]" << std::endl;
          },
          [&out, &msg, this](td_api::messageChatJoinByLink &content) {
            out << "[type: Join] [ A new member joined the chat via an invite link. ]"
                << std::endl;
          },
          [&out, &msg, this](td_api::messageChatJoinByRequest &content) {
            out << "[type: Join] [ A new member was accepted to the chat by an administrator. ]"
                << std::endl;
          },
          [&out](auto &content) {
            out << "[text: Unsupported" << "]" << std::endl;
          }
        )
    );
  }
}

void TdShell::cmdOpenContent(std::ostream& out, int64_t chat_id, int64_t msg_id) {
  std::promise<bool> prom;
  core_->send_query(td_api::make_object<td_api::openMessageContent >(chat_id, msg_id),
    [this, &prom](ObjectPtr object)
    {
      if (object->get_id() == td_api::error::ID) {
        prom.set_exception(std::make_exception_ptr(
          std::logic_error("openMessageContent  failed")));
        return;
      }

      prom.set_value(true);
  });

  prom.get_future().wait();
}

void TdShell::error(std::ostream& out, std::string msg) {
  out << termcolor::red << msg << termcolor::reset << std::endl;
}

void TdShell::cmdReadFile(std::ostream& out, int64_t chat_id, int64_t msg_id) {
  auto filenames = getFileIdFromMessages(chat_id, {msg_id});
  std::promise<FilePartPtr> prom;
  auto id = filenames.cbegin()->first;
  core_->send_query(td_api::make_object<td_api::readFilePart >(id, 0, 0),
    [this, &prom](ObjectPtr object)
    {
      if (object->get_id() == td_api::error::ID) {
        prom.set_exception(std::make_exception_ptr(
          std::logic_error("readFilePart  failed")));
        return;
      }

      prom.set_value(td::move_tl_object_as<td_api::filePart>(object));
  });

  FilePartPtr file = prom.get_future().get();
  out << filenames[0] << ": " << file->data_.size() << std::endl;
}

std::map<int32_t, std::string> TdShell::getFileIdFromMessages(
  int64_t chat_id, std::vector<int64_t> msg_ids) {

  std::vector<MessagePtr> MsgObjs;
  for (auto msg_id : msg_ids) {
    std::promise<MessagePtr> prom;
    auto fut = prom.get_future();
    core_->send_query(
      td_api::make_object<td_api::getMessage>(chat_id, msg_id),
      [this, &prom](ObjectPtr object) {
        if (object->get_id() == td_api::error::ID) {
          prom.set_exception(std::make_exception_ptr(std::logic_error("getMessage failed")));
          return;
        }

        prom.set_value(td::move_tl_object_as<td_api::message>(object));
      }
    );
    MsgObjs.push_back(std::move(fut.get()));
  }

  std::vector<std::promise<FilePtr>> promises;
  for (size_t i = 0; i < MsgObjs.size(); i++) {
    promises.push_back(std::promise<FilePtr>());
  }

  std::map<int32_t, std::string> filenames;

  for (size_t i = 0; i < MsgObjs.size(); i++) {
    auto &msg = MsgObjs[i];
    auto &prom = promises[i];

    std::int32_t file_id;
    std::string filename;
  
    td_api::downcast_call(
      *(msg->content_), overloaded(
        [this, &file_id, &filename](td_api::messageDocument &content) {
          file_id = content.document_->document_->id_;
          filename = content.document_->file_name_;
        },
        [this, &file_id, &filename](td_api::messageVideo &content) {
          file_id = content.video_->video_->id_;
          filename = content.video_->file_name_;
        },
        [](auto &content) {}
      )
    );

    filenames.insert({file_id, filename});
  }

  return filenames;
}