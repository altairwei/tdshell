#include "commands.h"

#include "tdchannel.h"

#define DESCRIPTION "download - Download file in messages"
CmdDownload::CmdDownload(std::shared_ptr<TdChannel> &channel)
  : Command("download", DESCRIPTION, channel) {
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
  try {
    parse(args);
  } catch (const CLI::ParseError &e) {
    app_->exit(e, out);
  }

  if (is_link_) {
    download(out, messages_);
  } else {
    std::vector<int64_t> ids;
    for (auto &s : messages_)
      ids.push_back(std::stol(s));
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