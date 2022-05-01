#include "commands.h"

#include "tdchannel.h"

/////////////////////////////////////////////////////////////////////////////
// CmdChats
/////////////////////////////////////////////////////////////////////////////

CmdDownload::CmdDownload(std::shared_ptr<TdChannel> &channel)
  : Program("download", "Download file in messages", channel) {
  app_->add_option("messages", messages_, "message ids or message links");
  auto link_opt = app_->add_flag("--link,-l", is_link_, "pass message links");
  app_->add_option("--chat-id,-i", chat_title_, "chat id or title")
      ->excludes(link_opt);
}

void CmdDownload::reset() {
  is_link_ = false;
  chat_title_.clear();
}

void CmdDownload::run(std::vector<std::string> args, std::ostream& out) {
  Program::run(args, out);
  if (is_link_) {
    download(out, messages_);
  } else {
    std::vector<int64_t> ids;
    for (auto &s : messages_) {
      try {
        ids.push_back(std::stol(s));
      } catch (std::invalid_argument const& ex) {
        throw std::logic_error("invalid message id: " + s);
      } catch (std::out_of_range const& ex) {
        throw std::logic_error("message id is out of range: " + s);
      }
    }

    if (chat_title_.empty())
      throw std::logic_error("Chat id or chat title should be provided.");

    download(out, chat_title_, ids);
  }
}

void CmdDownload::download(std::ostream& out, std::vector<std::string> links) {
  std::vector<MessagePtr> messages;
  for (auto &link : links) {
    auto info = channel_->invoke<td_api::getMessageLinkInfo>(link);
    messages.emplace_back(std::move(info->message_));
  }

  downloadFileInMessages(out, std::move(messages));
}

void CmdDownload::download(std::ostream& out, std::string chat, std::vector<int64_t> message_ids) {
  int64_t chat_id = channel_->getChatId(chat);
  download(out, chat_id, message_ids);
}

void CmdDownload::download(std::ostream& out, int64_t chat_id, std::vector<int64_t> message_ids) {
  std::vector<MessagePtr> MsgObjs;
  for (auto msg_id : message_ids) {
    MsgObjs.emplace_back(std::move(
      channel_->invoke<td_api::getMessage>(chat_id, msg_id)
    ));
  }

  downloadFileInMessages(out, std::move(MsgObjs));
}

void CmdDownload::downloadFileInMessages(std::ostream& out, std::vector<MessagePtr> messages) {
  std::vector<std::promise<bool>> promises;
  for (size_t i = 0; i < messages.size(); i++) {
    promises.emplace_back();
  }

  for (size_t i = 0; i < messages.size(); i++) {
    auto &msg = messages[i];
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
        [&](td_api::messagePhoto &content) {
          auto &ps = content.photo_->sizes_;
          auto ret = std::max_element(ps.begin(), ps.end(), [] (auto &a, auto &b) {
            return a->photo_->expected_size_ < b->photo_->expected_size_;
          });
          auto &ph = *ret;
          file_id = ph->photo_->id_;
          filename = content.caption_->text_;
          can_be_downloaded = ph->photo_->local_->can_be_downloaded_;
          is_downloading_completed = ph->photo_->local_->is_downloading_completed_;
        },
        [](auto &content) {}
      )
    );

    if (!can_be_downloaded)
      throw std::logic_error("File can't be download: " + filename);

    if (!is_downloading_completed) {
      channel_->addDownloadHandler(file_id, [&out, &prom, filename](FilePtr file) {
        printProgress(out, filename, file->expected_size_, file->local_->downloaded_size_);
        if (file->local_->is_downloading_completed_) {
          out << std::endl;
          out << file->local_->path_ << std::endl;
          prom.set_value(true);
        }
      });

      channel_->invoke<td_api::downloadFile>(file_id, 32, 0, 0, false);
    } else {
      prom.set_value(true);
    }
  }

  for (auto &prom : promises) {
    prom.get_future().wait();
  }
}

/////////////////////////////////////////////////////////////////////////////
// CmdChats
/////////////////////////////////////////////////////////////////////////////

CmdChats::CmdChats(std::shared_ptr<TdChannel> &channel)
  : Program("chats", "Get chat dialog list", channel) {
  app_->add_option("--limit,-l", limit_, "The maximum number of chats to be returned");
}

void CmdChats::run(std::vector<std::string> args, std::ostream& out) {
  Program::run(args, out);
  auto chats = channel_->invoke<td_api::getChats>(nullptr, limit_);
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

          //out << "[super_id: " << type.supergroup_id_ << "] ";
        },
        [&out](td_api::chatTypePrivate &type) {
          out << "👤 ";
          //out << "[user_id: " << type.user_id_ << "] ";
        },
        [&out](td_api::chatTypeSecret &type) {
          out << "👤 ";
          //out << "[secret_id: " << type.secret_chat_id_ << "] ";
        },
        [&out](td_api::chatTypeBasicGroup &type) {
          out << "🙌 ";
          //out << "[basic_id: " << type.basic_group_id_ << "] ";
        }
      )
    );

    out << "[chat_id: " << chat_id << "] ";
    out << chat->title_;
    out << std::endl;
  }
}

void CmdChats::reset() {
  limit_ = std::numeric_limits<int32_t>::max();
}

/////////////////////////////////////////////////////////////////////////////
// CmdChatInfo
/////////////////////////////////////////////////////////////////////////////

CmdChatInfo::CmdChatInfo(std::shared_ptr<TdChannel> &channel)
  : Program("chatinfo", "Get information of a chat", channel) {
  app_->add_option("chat", chat_, "chat id or title");
}

void CmdChatInfo::reset() {
  chat_.clear();
}

void CmdChatInfo::run(std::vector<std::string> args, std::ostream& out) {
  Program::run(args, out);

  int64_t chat_id = channel_->getChatId(chat_);
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

/////////////////////////////////////////////////////////////////////////////
// CmdHistory
/////////////////////////////////////////////////////////////////////////////

CmdHistory::CmdHistory(std::shared_ptr<TdChannel> &channel)
  : Program("history", "Get the history of a chat", channel) {
  app_->add_option("chat", chat_, "Chat id or title.");
  app_->add_option("--date,-d", date_, "Get history no later than the specified date (ISO format).");
  app_->add_option("--limit,-l", limit_, "	The maximum number of messages to be returned.");
}

void CmdHistory::reset() {
  chat_.clear();
  limit_ = 50;
  date_.clear();
}

void CmdHistory::run(std::vector<std::string> args, std::ostream& out) {
  Program::run(args, out);

  if (!date_.empty()) {
    history(out, chat_, date_, limit_);
  } else {
    history(out, chat_, limit_);
  }
}

void CmdHistory::history(std::ostream& out, std::string chat_title, uint limit)
{
  int64_t chat_id = channel_->getChatId(chat_title);
  history(out, chat_id, limit);
}

void CmdHistory::history(std::ostream& out, std::string chat_title, std::string date, uint limit)
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

void CmdHistory::history(std::ostream& out, int64_t chat_id, uint limit) {
  std::promise<MessageListPtr> prom;
  auto fut = prom.get_future();
  auto chat = channel_->invoke<td_api::getChat>(chat_id);
  auto messages = channel_->invoke<td_api::getChatHistory>(
    chat_id, chat->last_message_->id_, -1, limit, false);
  for (auto &msg : messages->messages_) {
    printMessage(out, msg);
  }
}

/////////////////////////////////////////////////////////////////////////////
// CmdHistory
/////////////////////////////////////////////////////////////////////////////

CmdMessageLink::CmdMessageLink(std::shared_ptr<TdChannel> &channel)
  : Program("messagelink", "Get message from link", channel) {
  app_->add_option("link", link_, "Message or post link.");
}

void CmdMessageLink::reset() {
  link_.clear();
}

void CmdMessageLink::run(std::vector<std::string> args, std::ostream& out) {
  Program::run(args, out);
  auto info = channel_->invoke<td_api::getMessageLinkInfo>(link_);
  printMessage(out, info->message_);
}
