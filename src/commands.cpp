#include "commands.h"

#include <filesystem>
#include <fstream>
#include <nowide/cstdio.hpp>
#include <nowide/fstream.hpp>
#include <nowide/quoted.hpp>

#include "tdchannel.h"
#include "utils.h"

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////////////
// CmdChats
/////////////////////////////////////////////////////////////////////////////

CmdDownload::CmdDownload(std::shared_ptr<TdChannel> &channel)
  : Program("download", "Download file in messages", channel) {
  auto opt_links = app_->add_option("--links,-l", links_, "Use message links to download");
  auto opt_ids = app_->add_option("--ids,-i", msg_ids_, "Use message IDs to download, require chat title");

  auto opt_input_file = app_->add_option("--input-file,-f", input_file_,
                   "The file should consist of a series of "
                   "message ids/links, one per line");

  auto opt_range = app_->add_option("-R,--range", range_,
                    "Find messages between <from,to> or <from to> links.")
      ->expected(2)->delimiter(',');

  auto opt_chat_title = app_->add_option("--chat-id,-t", chat_title_, "chat id or title, "
                   "if specified the content of options `-R` and `-f` "
                   "will be interpreted as message IDs.");
  app_->add_option("--output-folder,-O", output_folder_,
                   "Put downloaded files to a given folder.");

  opt_ids->needs(opt_chat_title);
  opt_ids->excludes(opt_links);

  opt_input_file->excludes(opt_links, opt_ids);
  opt_range->excludes(opt_input_file, opt_links, opt_ids);

  opt_chat_title->excludes(opt_links);
}

void CmdDownload::reset() {
  chat_title_.clear();
  links_.clear();
  msg_ids_.clear();
  input_file_.clear();
  output_folder_ = fs::current_path().u8string();
  range_.clear();
}

void CmdDownload::run(std::ostream& out) {
  if (!input_file_.empty()) {
    // Get links or IDs from file.
    parseMessagesInFile(input_file_);
  }

  if (!links_.empty())
    download(out, links_);

  if (!msg_ids_.empty()) {

    std::vector<int64_t> ids;
    for (auto &s : msg_ids_) {
      try {
        ids.push_back(std::stoll(s));
      } catch (std::invalid_argument const&) {
        throw std::logic_error("invalid message id: " + s);
      } catch (std::out_of_range const&) {
        throw std::logic_error("message id is out of range: " + s);
      }
    }

    if (chat_title_.empty())
      throw std::logic_error("Chat id or chat title should be provided.");

    download(out, chat_title_, ids);
  }

  if (!range_.empty())
    downloadMessagesInRange(out);
}

void CmdDownload::parseMessagesInFile(const std::string &filename)
{
  fs::path file(filename);
  if (!fs::exists(file))
    throw std::logic_error(filename + " dosn't exist.");

  std::ifstream infile(file);
  std::string line;
  while (std::getline(infile, line)) {
    std::string msg = StrUtil::trim(line);
    if (!msg.empty()) {
      if (chat_title_.empty())
        links_.push_back(msg);
      else
        msg_ids_.push_back(msg);
    }
  }
}

void CmdDownload::downloadMessagesInRange(std::ostream& out)
{
  if (range_.empty()) return;

  MessagePtr from_msg = nullptr, to_msg = nullptr;
  // Use chat_title_ to determine how to interpret range_
  if (chat_title_.empty()) {
    from_msg = std::move(channel_->invoke<td_api::getMessageLinkInfo>(range_.front())->message_);
    to_msg = std::move(channel_->invoke<td_api::getMessageLinkInfo>(range_.back())->message_);
  } else {
    int64_t chat_id = channel_->getChatId(chat_title_);
    from_msg = channel_->invoke<td_api::getMessage>(chat_id, std::stoll(range_.front()));
    to_msg = channel_->invoke<td_api::getMessage>(chat_id, std::stoll(range_.back()));
  }

  auto messages = channel_->getMessageForRange(std::move(from_msg), std::move(to_msg));
  downloadFileInMessages(out, std::move(messages));
}

static bool isDownloadableMsg(const MessagePtr &msg) {
  switch (msg->content_->get_id())
  {
  case td_api::messageDocument::ID:
    return true;
  case td_api::messageVideo::ID:
    return true;
  case td_api::messagePhoto::ID:
    return true;
  default:
    return false;
  }
}

void CmdDownload::download(std::ostream& out, std::vector<std::string> links) {
  std::vector<MessagePtr> messages;
  for (auto &link : links) {
    auto info = channel_->invoke<td_api::getMessageLinkInfo>(link);
    if (!isDownloadableMsg(info->message_)) {
      out << "unsupported message: " << link << std::endl;
      continue;
    }
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
    MessagePtr msg = channel_->invoke<td_api::getMessage>(chat_id, msg_id);
    if (!isDownloadableMsg(msg)) {
      out << "unsupported message: " << msg_id << std::endl;
      continue;
    }
    MsgObjs.emplace_back(std::move(msg));
  }

  downloadFileInMessages(out, std::move(MsgObjs));
}

struct DownloadTask {
  std::int32_t file_id;
  std::string filename;
  bool can_be_downloaded;
  bool is_downloading_completed;
  FilePtr file;

  // Default constructor
  DownloadTask() = delete;

  // Constructor to initialize the struct with values
  DownloadTask(std::int32_t id, const std::string& name, bool can_download,
                bool is_download_completed, FilePtr f)
    : file_id(id), filename(name), can_be_downloaded(can_download),
      is_downloading_completed(is_download_completed), file(std::move(f)) {}

  // Move constructor
  DownloadTask(DownloadTask&& other) noexcept
    : file_id(other.file_id),
      filename(std::move(other.filename)),
      can_be_downloaded(other.can_be_downloaded),
      is_downloading_completed(other.is_downloading_completed),
      file(std::move(other.file)) { }

  // Move assignment operator
  DownloadTask& operator=(DownloadTask&& other) noexcept {
    if (this != &other) {
      file_id = other.file_id;
      filename = std::move(other.filename);
      can_be_downloaded = other.can_be_downloaded;
      is_downloading_completed = other.is_downloading_completed;
      file = std::move(other.file);
    }
    return *this;
  }
};

void CmdDownload::downloadFileInMessages(std::ostream& out, std::vector<MessagePtr> messages) {
  // Sort messages in descending order
  std::sort(messages.begin(), messages.end(),
    [](const MessagePtr& a, const MessagePtr& b) { return a->id_ > a->id_; });

  // Extract media in messages
  std::vector<DownloadTask> tasks;
  for (size_t i = 0; i < messages.size(); i++) {
    auto &msg = messages[i];

    td_api::downcast_call(
      *(msg->content_), overloaded(
        [&](td_api::messageDocument &content) {
          tasks.emplace(tasks.end(),
            content.document_->document_->id_,
            content.document_->file_name_,
            content.document_->document_->local_->can_be_downloaded_,
            content.document_->document_->local_->is_downloading_completed_,
            std::move(content.document_->document_)
          );
        },
        [&](td_api::messageVideo &content) {
          tasks.emplace(tasks.end(),
            content.video_->video_->id_,
            content.video_->file_name_,
            content.video_->video_->local_->can_be_downloaded_,
            content.video_->video_->local_->is_downloading_completed_,
            std::move(content.video_->video_)
          );
        },
        [&](td_api::messagePhoto &content) {
          auto &ps = content.photo_->sizes_;
          auto ret = std::max_element(ps.begin(), ps.end(), [] (auto &a, auto &b) {
            return a->photo_->expected_size_ < b->photo_->expected_size_;
          });
          auto &ph = *ret;
          tasks.emplace(tasks.end(),
            ph->photo_->id_,
            content.caption_->text_,
            ph->photo_->local_->can_be_downloaded_,
            ph->photo_->local_->is_downloading_completed_,
            std::move(ph->photo_)
          );
        },
        [](auto &content) {/* Unsupported message. */}
      )
    );
  }

  size_t ntasks = tasks.size();
  size_t skipped = messages.size() - ntasks;

  // Remove duplicated files
  std::sort(
    tasks.begin(), tasks.end(),
    [](const DownloadTask& a, const DownloadTask& b) { return a.file_id > b.file_id; });
  auto last = std::unique(
    tasks.begin(), tasks.end(),
    [](const DownloadTask& a, const DownloadTask& b) { return a.file_id == b.file_id; });
  tasks.erase(last, tasks.end());

  size_t duplicated = ntasks - tasks.size();

  if (tasks.size() > 1) {
    out << "Total " << tasks.size() << " files to be downloaded";
    if (skipped > 0)
      out << ", " << skipped << (skipped > 1 ? " messages" : " message") << " skipped";
    if (duplicated > 0)
      out << ", " << duplicated << " duplicated" << (duplicated > 1 ? " files" : " file") << " skipped";
    out << ":" << std::endl;
  }

  std::vector<std::promise<FilePtr>> promises{tasks.size()};
  std::vector<std::future<FilePtr>> futures;

  for (size_t i = 0; i < tasks.size(); i++){
    auto &task = tasks[i];
    auto &prom = promises[i];

    if (!task.can_be_downloaded)
      throw std::logic_error("File can't be download: " + task.filename);

    if (!task.is_downloading_completed) {
      channel_->addDownloadHandler(task.file_id, [&out, &prom, &task](FilePtr file) {
        std::lock_guard<std::mutex> guard{ConsoleUtil::output_lock};
        ConsoleUtil::printProgress(out, task.filename, file->expected_size_, file->local_->downloaded_size_);
        if (file->local_->is_downloading_completed_) {
          out << std::endl;
          prom.set_value(std::move(file));
        }
      });

      channel_->invoke<td_api::downloadFile>(task.file_id, 32, 0, 0, false);
    } else {
      prom.set_value(std::move(task.file));
    }

    futures.push_back(prom.get_future());
  }

  AsynUtil::waitFutures<FilePtr>(futures, [this, &out] (FilePtr file, size_t i) {
    channel_->removeDownloadHandler(file->id_);

    fs::path localfile = fs::u8path(file->local_->path_);
    fs::path destfile = fs::u8path(output_folder_);

    if (!std::filesystem::exists(destfile)) {
        if (!std::filesystem::create_directory(destfile)) {
          throw std::runtime_error("Failed to create directory " + output_folder_);
        }
    }

    destfile /= localfile.filename();
    fs::rename(localfile, destfile);

    std::lock_guard<std::mutex> guard{ConsoleUtil::output_lock};
    out << fs::absolute(destfile).u8string() << std::endl;
  });

}

/////////////////////////////////////////////////////////////////////////////
// CmdChats
/////////////////////////////////////////////////////////////////////////////

CmdChats::CmdChats(std::shared_ptr<TdChannel> &channel)
  : Program("chats", "Get chat dialog list", channel) {
  app_->add_option("--limit,-l", limit_, "The maximum number of chats to be returned");
  app_->add_flag("--archive,-R", archive_list_, "Show archived chats.");
  app_->add_option("--filter-id,-F", chat_filter_id_, "Show chats in a folder by filter identifier.");
}

void CmdChats::run(std::ostream& out) {
  ChatListPtr chats;
  if (chat_filter_id_ != 0) {
    chats = channel_->invoke<td_api::getChats>(
      td_api::make_object<td_api::chatListFilter>(chat_filter_id_), limit_);
  } else if (archive_list_) {
    chats = channel_->invoke<td_api::getChats>(
      td_api::make_object<td_api::chatListArchive>(), limit_);
  } else {
    chats = channel_->invoke<td_api::getChats>(nullptr, limit_);
  }

  for (auto chat_id : chats->chat_ids_) {
    auto chat = channel_->invoke<td_api::getChat>(chat_id);

    td_api::downcast_call(
      *(chat->type_), overloaded(
        [&out](td_api::chatTypeSupergroup &type) {
          if (type.is_channel_) {
            out << u8"📢 ";
          } else {
            out << u8"👥 ";
          }

          //out << "[super_id: " << type.supergroup_id_ << "] ";
        },
        [&out](td_api::chatTypePrivate &type) {
          out << u8"👤 ";
          //out << "[user_id: " << type.user_id_ << "] ";
        },
        [&out](td_api::chatTypeSecret &type) {
          out << u8"👤 ";
          //out << "[secret_id: " << type.secret_chat_id_ << "] ";
        },
        [&out](td_api::chatTypeBasicGroup &type) {
          out << u8"🙌 ";
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
  archive_list_ = false;
  chat_filter_id_ = 0;
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

void CmdChatInfo::run(std::ostream& out) {
  int64_t chat_id = channel_->getChatId(chat_);
  ChatPtr chat = channel_->invoke<td_api::getChat>(chat_id);

  td_api::downcast_call(
    *(chat->type_), overloaded(
      [&out](td_api::chatTypeSupergroup &type) {
        if (type.is_channel_) {
          out << u8"📢 ";
        } else {
          out << u8"👥 ";
        }

        out << "[super_id: " << type.supergroup_id_ << "] ";
      },
      [&out](td_api::chatTypePrivate &type) {
        out << u8"👤 ";
        out << "[user_id: " << type.user_id_ << "] ";
      },
      [&out](td_api::chatTypeSecret &type) {
        out << u8"👤 ";
        out << "[secret_id: " << type.secret_chat_id_ << "] ";
      },
      [&out](td_api::chatTypeBasicGroup &type) {
        out << u8"🙌 ";
        out << "[basic_id: " << type.basic_group_id_ << "] ";
      }
    )
  );

  out << "[chat_id: " << chat->id_ << "] ";
  out << chat->title_;
  out << std::endl;

  out << "- last_message: " << chat->last_message_->id_ << std::endl;
  out << "---- ";
  ConsoleUtil::printMessage(out, chat->last_message_);
}

/////////////////////////////////////////////////////////////////////////////
// CmdHistory
/////////////////////////////////////////////////////////////////////////////

CmdHistory::CmdHistory(std::shared_ptr<TdChannel> &channel)
  : Program("history", "Get the history of a chat", channel) {
  app_->add_option("chat", chat_, "Chat id or title.");
  app_->add_option("--date,-d", date_, "Get history no later than the specified date (ISO format).");
  app_->add_option("--limit,-l", limit_, "The maximum number of messages to be returned.");
  app_->add_option("--from-message,-f", from_, "Get history older than the given message (id/link).");
}

void CmdHistory::reset() {
  chat_.clear();
  limit_ = 50;
  date_.clear();
  from_.clear();
}

void CmdHistory::run(std::ostream& out) {
  if (!date_.empty()) {
    history(out, chat_, date_, limit_);
  } else if (!from_.empty()) {
    if (!chat_.empty()) {
      try {
        int64_t chat_id = channel_->getChatId(chat_);
        auto msg = channel_->invoke<td_api::getMessage>(chat_id, std::stoll(from_));
        history(out, msg, limit_);
      } catch (std::invalid_argument const&) {
        throw std::runtime_error("`--from-message` must be an ID not a link when chat title provided.");
      }
    } else {
      history(out, channel_->invoke<td_api::getMessageLinkInfo>(from_)->message_, limit_);
    }
  } else {
    history(out, chat_, limit_);
  }
}

void CmdHistory::history(std::ostream& out, std::string chat_title, int32_t limit)
{
  int64_t chat_id = channel_->getChatId(chat_title);
  history(out, chat_id, limit);
}

void CmdHistory::history(std::ostream& out, std::string chat_title, std::string date, int32_t limit)
{
  int64_t chat_id = channel_->getChatId(chat_title);
  std::tm t = {};
  std::istringstream ss(date);
  // FIXME: make use of Time.h of tdutils
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
    ConsoleUtil::printMessage(out, msg);
  }
}

void CmdHistory::history(std::ostream& out, int64_t chat_id, int32_t limit) {
  std::promise<MessageListPtr> prom;
  auto fut = prom.get_future();
  auto chat = channel_->invoke<td_api::getChat>(chat_id);
  auto messages = channel_->invoke<td_api::getChatHistory>(
    chat_id, chat->last_message_->id_, -1, limit, false);
  for (auto &msg : messages->messages_) {
    ConsoleUtil::printMessage(out, msg);
  }
}

void CmdHistory::history(std::ostream& out, MessagePtr& msg, int32_t limit)
{
  auto messages = channel_->invoke<td_api::getChatHistory>(
    msg->chat_id_, msg->id_, -1, limit, false);
  for (auto &msg : messages->messages_) {
    ConsoleUtil::printMessage(out, msg);
  }
}

/////////////////////////////////////////////////////////////////////////////
// CmdHistory
/////////////////////////////////////////////////////////////////////////////

CmdMessageLink::CmdMessageLink(std::shared_ptr<TdChannel> &channel)
  : Program("messagelink", "Get message from link", channel) {
  auto link_opt = app_->add_option("link", link_, "Message or post link.");
  auto file_opt = app_->add_option("--input-file,-f", input_file_,
                   "The file should consist of a series of "
                   "message ids/links, one per line")
      ->excludes(link_opt);
  auto range_opt = app_->add_option("-R,--range", range_,
                    "Look up messages between <from,to> or <from to> links.")
      ->expected(2)->delimiter(',')
      ->excludes(file_opt)->excludes(link_opt);
}

void CmdMessageLink::reset() {
  link_.clear();
  input_file_.clear();
  range_.clear();
}

void CmdMessageLink::run(std::ostream& out) {
  if (!link_.empty()) {
    auto info = channel_->invoke<td_api::getMessageLinkInfo>(link_);
    ConsoleUtil::printMessage(out, info->message_);
  }

  if (!input_file_.empty()) {
    nowide::ifstream f(input_file_);
    if(!f) throw std::logic_error("Can't open " + input_file_);

    std::vector<std::string> links;
    std::string line;
    while (std::getline(f, line)) {
      std::string msg = StrUtil::trim(line);
      if (!msg.empty())
        links.push_back(msg);
    }

    f.close();

    for (auto &li : links) {
      auto info = channel_->invoke<td_api::getMessageLinkInfo>(li);
      ConsoleUtil::printMessage(out, info->message_);
    }
  }

  if (!range_.empty()) {
    MessagePtr from_msg = std::move(channel_->invoke<td_api::getMessageLinkInfo>(range_.front())->message_);
    MessagePtr to_msg = std::move(channel_->invoke<td_api::getMessageLinkInfo>(range_.back())->message_);
    auto messages = channel_->getMessageForRange(std::move(from_msg), std::move(to_msg));
    for (auto &msg : messages)
      ConsoleUtil::printMessage(out, msg);
  }
}
