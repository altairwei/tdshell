#include "tdshell.h"

#include <iostream>
#include <sstream>
#include <locale>
#include <iomanip>
#include <algorithm>

#include <cli/detail/rang.h>
#include <termcolor/termcolor.hpp>
#include <td/telegram/td_api.hpp>

#include "common.h"

using namespace cli;

TdShell::TdShell() {
  channel_ = std::make_shared<TdChannel>();
  commands_["download"] = std::make_unique<CmdDownload>(channel_);
}

void TdShell::open() {
  channel_->start();
  channel_->waitLogin();
}

void TdShell::close() {
  channel_->stop();
}

void TdShell::execute(std::string cmd, std::vector<std::string> &args, std::ostream &out) {
  commands_[cmd]->run(args, out);
}

std::unique_ptr<Menu> TdShell::make_menu() {
  auto rootMenu = std::make_unique<Menu>("tdshell");
  rootMenu->Insert(
    "chats",
    [this](std::ostream& out) { cmdChats(out); },
    "Get chat list.");
  rootMenu->Insert(
    "chat_info",
    [this](std::ostream& out, std::string chat) { cmdChatInfo(out, chat); },
    "Get chat list.");
  rootMenu->Insert(
    "history",
    [this](std::ostream& out, std::string chat_title, uint limit) {
      cmdHistory(out, chat_title, limit);
    },
    "Get the history of a chat.",
    {"chat title or id", "limit"}
  );
  rootMenu->Insert(
    "history",
    [this](std::ostream& out, std::string chat_title, std::string date, uint limit) {
      cmdHistory(out, chat_title, date, limit);
    },
    "Get the history of a chat start from given date (such as 2022-04-17).",
    {"chat title or id", "date string", "limit"}
  );
  rootMenu->Insert(
    "download",
    [this](std::ostream& out, std::vector<std::string> args) {
      execute("download", args, out);
    },
    "Download files in messages.",
    {"arguments"}
  );
  rootMenu->Insert(
    "messagelink",
    [this](std::ostream& out, std::string link) {
      auto msg = getMessageByLink(link);
      printMessage(out, msg);
    },
    "Get message from link.",
    {"chat_id", "message_ids seperated by comma"}
  );

  return std::move(rootMenu);
}

void TdShell::cmdChats(std::ostream& out) {
  auto chats = channel_->invoke<td_api::getChats>(nullptr, std::numeric_limits<int32_t>::max());
  for (auto chat_id : chats->chat_ids_) {
    auto chat = channel_->invoke<td_api::getChat>(chat_id);

    td_api::downcast_call(
      *(chat->type_), overloaded(
        [&out](td_api::chatTypeSupergroup &type) {
          if (type.is_channel_) {
            out << "📢 ";
          } else {
            out << "👥 ";
          }

          out << "[super_id: " << type.supergroup_id_ << "] ";
        },
        [&out](td_api::chatTypePrivate &type) {
          out << "👤 ";
          out << "[user_id: " << type.user_id_ << "] ";
        },
        [&out](td_api::chatTypeSecret &type) {
          out << "👤 ";
          out << "[secret_id: " << type.secret_chat_id_ << "] ";
        },
        [&out](td_api::chatTypeBasicGroup &type) {
          out << "🙌 ";
          out << "[basic_id: " << type.basic_group_id_ << "] ";
        }
      )
    );

    out << "[chat_id: " << chat_id << "] ";
    out << chat->title_;
    out << std::endl;
  }
}

void TdShell::cmdChatInfo(std::ostream& out, std::string chat_title) {
  int64_t chat_id = channel_->getChatId(chat_title);
  ChatPtr chat = channel_->invoke<td_api::getChat>(chat_id);

  td_api::downcast_call(
    *(chat->type_), overloaded(
      [&out](td_api::chatTypeSupergroup &type) {
        if (type.is_channel_) {
          out << "📢 ";
        } else {
          out << "👥 ";
        }

        out << "[super_id: " << type.supergroup_id_ << "] ";
      },
      [&out](td_api::chatTypePrivate &type) {
        out << "👤 ";
        out << "[user_id: " << type.user_id_ << "] ";
      },
      [&out](td_api::chatTypeSecret &type) {
        out << "👤 ";
        out << "[secret_id: " << type.secret_chat_id_ << "] ";
      },
      [&out](td_api::chatTypeBasicGroup &type) {
        out << "🙌 ";
        out << "[basic_id: " << type.basic_group_id_ << "] ";
      }
    )
  );

  out << "[chat_id: " << chat->id_ << "] ";
  out << chat->title_;
  out << std::endl;

  out << "- last_message: " << chat->last_message_->id_ << std::endl;
  out << "---- ";
  printMessage(out, chat->last_message_);
}

void TdShell::cmdHistory(std::ostream& out, std::string chat_title, uint limit)
{
  int64_t chat_id = channel_->getChatId(chat_title);
  cmdHistory(out, chat_id, limit);
}

void TdShell::cmdHistory(std::ostream& out, std::string chat_title, std::string date, uint limit)
{
  int64_t chat_id = channel_->getChatId(chat_title);
  std::tm t = {};
  std::istringstream ss(date);
  ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail()) {
      throw std::logic_error("Parse date failed");
  }

  time_t timestamp = std::mktime(&t);

  std::promise<MessagePtr> prom1;
  channel_->make_query<td_api::getChatMessageByDate>(prom1, chat_id, (int32_t) timestamp);
  auto msg = prom1.get_future().get();

  std::promise<MessageListPtr> prom2;
  channel_->make_query<td_api::getChatHistory>(prom2, chat_id, msg->id_, -1, limit, false);
  MessageListPtr messages = prom2.get_future().get();

  for (auto &msg : messages->messages_) {
    printMessage(out, msg);
  }
}

void TdShell::cmdHistory(std::ostream& out, int64_t chat_id, uint limit) {
  std::promise<MessageListPtr> prom;
  auto fut = prom.get_future();
  auto chat = channel_->invoke<td_api::getChat>(chat_id);
  auto messages = channel_->invoke<td_api::getChatHistory>(
    chat_id, chat->last_message_->id_, -1, limit, false);
  for (auto &msg : messages->messages_) {
    printMessage(out, msg);
  }
}

void TdShell::cmdOpenContent(std::ostream& out, int64_t chat_id, int64_t msg_id) {
  std::promise<bool> prom;
  channel_->send_query(td_api::make_object<td_api::openMessageContent >(chat_id, msg_id),
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
  channel_->send_query(td_api::make_object<td_api::readFilePart >(id, 0, 0),
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
    channel_->make_query<td_api::getMessage>(prom, chat_id, msg_id);
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

MessagePtr TdShell::getMessageByLink(std::string link) {
  auto info = channel_->invoke<td_api::getMessageLinkInfo>(link);
  return std::move(info->message_);
}