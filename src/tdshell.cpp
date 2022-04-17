#include "tdshell.h"

#include <iostream>
#include <sstream>
#include <locale>
#include <iomanip>

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

void TdShell::cmdDownload(
    std::ostream& out,
    int64_t chat_id,
    std::vector<int64_t> &message_ids) {

  std::vector<MessagePtr> MsgObjs;
  for (auto msg_id : message_ids) {
    MsgObjs.emplace_back(std::move(
      core_->invoke<td_api::getMessage>(chat_id, msg_id)
    ));
  }

  std::vector<std::promise<bool>> promises;
  for (size_t i = 0; i < MsgObjs.size(); i++) {
    promises.emplace_back();
  }

  for (size_t i = 0; i < MsgObjs.size(); i++) {
    auto &msg = MsgObjs[i];
    auto &prom = promises[i];

    std::int32_t file_id;
    std::string filename;
    bool can_be_downloaded;
    bool is_downloading_completed;

    td_api::downcast_call(
      *(msg->content_), overloaded(
        [&](td_api::messageDocument &content) {
          file_id = content.document_->document_->id_;
          filename = content.document_->file_name_;
          can_be_downloaded = content.document_->document_->local_->can_be_downloaded_;
          is_downloading_completed = content.document_->document_->local_->is_downloading_completed_;
        },
        [&](td_api::messageVideo &content) {
          file_id = content.video_->video_->id_;
          filename = content.video_->file_name_;
          can_be_downloaded = content.video_->video_->local_->can_be_downloaded_;
          is_downloading_completed = content.video_->video_->local_->is_downloading_completed_;
        },
        [](auto &content) {}
      )
    );

    if (!can_be_downloaded)
      throw std::logic_error("File can't be download: " + filename);

    if (is_downloading_completed) {
      prom.set_value(true);
    } else {
      core_->addDownloadHandler(file_id, [&out, &prom, filename](FilePtr file) {
        printProgress(out, filename, file->expected_size_, file->local_->downloaded_size_);
        if (file->local_->is_downloading_completed_) {
          out << std::endl;
          out << file->local_->path_ << std::endl;
          prom.set_value(true);
        }
      });

      core_->invoke<td_api::downloadFile>(file_id, 32, 0, 0, false);
    }
  }

  for (auto &prom : promises) {
    prom.get_future().wait();
  }
}

void TdShell::cmdHistory(std::ostream& out, std::string chat_title, uint limit)
{
  int64_t chat_id;

  try {
    chat_id = std::stoll(chat_title);
  } catch (std::invalid_argument const& e) {
    chat_id = core_->get_chat_id(chat_title);
    if (chat_id == 0) {
      error(out, "Not found chat " + chat_title);
      error(out, "Please use command 'chats' to update chat list.");
      return;
    }
  }

  cmdHistory(out, chat_id, limit);
}

void TdShell::cmdHistory(std::ostream& out, std::string chat_title, std::string date, uint limit)
{
  int64_t chat_id;

  try {
    chat_id = std::stoll(chat_title);
  } catch (std::invalid_argument const& e) {
    chat_id = core_->get_chat_id(chat_title);
    if (chat_id == 0) {
      error(out, "Not found chat " + chat_title);
      error(out, "Please use command 'chats' to update chat list.");
      return;
    }
  }

  std::tm t = {};
  std::istringstream ss(date);
  ss >> std::get_time(&t, "%Y-%m-%d");
  if (ss.fail()) {
      throw std::logic_error("Parse date failed");
  }

  time_t timestamp = std::mktime(&t);

  std::promise<MessagePtr> prom1;
  core_->make_query<td_api::getChatMessageByDate>(prom1, chat_id, timestamp);
  auto msg = prom1.get_future().get();

  std::promise<MessagesPtr> prom2;
  core_->make_query<td_api::getChatHistory>(prom2, chat_id, msg->id_, -1, limit, false);
  MessagesPtr messages = prom2.get_future().get();

  printMessages(out, std::move(messages));
}

void TdShell::cmdHistory(std::ostream& out, int64_t chat_id, uint limit) {
  std::promise<MessagesPtr> prom;
  auto fut = prom.get_future();
  core_->getChatHistory(prom, chat_id, limit == 0 ? 50 : limit);
  auto messages = fut.get();
  printMessages(out, std::move(messages));
}

void TdShell::printMessages(std::ostream& out, MessagesPtr messages) {
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
    core_->make_query<td_api::getMessage>(prom, chat_id, msg_id);
    MsgObjs.push_back(std::move(prom.get_future().get()));
  }

  std::map<int32_t, std::string> filenames;
  for (size_t i = 0; i < MsgObjs.size(); i++) {
    auto &msg = MsgObjs[i];

    std::int32_t file_id;
    std::string filename;
  
    td_api::downcast_call(
      *(msg->content_), overloaded(
        [&](td_api::messageDocument &content) {
          file_id = content.document_->document_->id_;
          filename = content.document_->file_name_;
        },
        [&](td_api::messageVideo &content) {
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