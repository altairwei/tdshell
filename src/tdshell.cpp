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
  commands_["chats"] = std::make_unique<CmdChats>(channel_);
  commands_["chatinfo"] = std::make_unique<CmdChatInfo>(channel_);
  commands_["history"] = std::make_unique<CmdHistory>(channel_);
  commands_["messagelink"] = std::make_unique<CmdMessageLink>(channel_);
}

TdShell::~TdShell() {
  close();
}

void TdShell::open() {
  channel_->waitForLogin();
  channel_->start();
}

void TdShell::close() {
  channel_->stop();
}

void TdShell::execute(std::string cmd, std::vector<std::string> &args, std::ostream &out) {
  commands_[cmd]->execute(args, out);
}

std::unique_ptr<Menu> TdShell::make_menu() {
  auto rootMenu = std::make_unique<Menu>("tdshell");

  for (auto &pair : commands_) {
    auto cmdname = pair.first;
    rootMenu->Insert(
      pair.first,
      [this, cmdname](std::ostream& out, std::vector<std::string> args) {
        execute(cmdname, args, out);
      },
      pair.second->description(),
      {"arguments"}
    );
  }

  return std::move(rootMenu);
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
