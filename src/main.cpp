#include "tdcore.h"

#include <iostream>
#include <sstream>

#include "common.h"

class TdShell {
public:
  TdShell() {
    core_ = std::make_unique<TdCore>();
    core_->start();
  }

  void loop() {
    core_->waitLogin();
    while (true) {
      std::string line;
      {
        std::lock_guard<std::mutex> guard{output_lock};
        std::cout << "tdshell > ";
        std::getline(std::cin, line);
      }
      std::istringstream ss(line);
      std::string action;
      if (!(ss >> action)) {
        continue;
      }

      if (action == "quit" || action == "exit") {
        core_->stop();
        return;
      }

      if (action == "chats") {
        std::promise<ChatsPtr> prom;
        auto fut = prom.get_future();
        core_->getChats(prom, 100);
        auto chats = fut.get();
        for (auto chat_id : chats->chat_ids_) {
          std::cout << "[chat_id: " << chat_id << "] [title: "
                    << core_->get_chat_title(chat_id) << "]" << std::endl;
        }
      } else if (action == "history") {
        std::int64_t chat_id;
        ss >> chat_id;
        std::uint32_t limit;
        ss >> limit;

        std::promise<MessagesPtr> prom;
        auto fut = prom.get_future();
        core_->getChatHistory(prom, chat_id, limit == 0 ? 50 : limit);
        auto messages = fut.get();
        for (auto &msg : messages->messages_) {
          std::int32_t msg_id = msg->content_->get_id();
          if (msg_id == td_api::messageText::ID) {
            std::string text = static_cast<td_api::messageText &>(*msg->content_).text_->text_;
            std::cout << "[msg_id: " << msg->id_ << "] [type: Text] [text: "
                      << text << "]" << std::endl;
          } else if (msg_id == td_api::messageVideo::ID) {
            auto &content = static_cast<td_api::messageVideo &>(*msg->content_);
            std::cout << "[msg_id: " << msg->id_ << "] [type: Video] [text: "
                      << content.caption_->text_ << "]" << std::endl;
          } else if (msg_id == td_api::messageDocument::ID) {
            auto &content = static_cast<td_api::messageDocument &>(*msg->content_);
            std::cout << "[msg_id: " << msg->id_ << "] [type: Document] [text: "
                      << content.document_->file_name_ << "]" << std::endl;
          } else if (msg_id == td_api::messagePhoto::ID) {
            auto &content = static_cast<td_api::messagePhoto &>(*msg->content_);
            std::cout << "[msg_id: " << msg->id_ << "] [type: Photo] [text: "
                      << content.caption_->text_ << "]" << std::endl;
          } else {
            std::cout << "[msg_id: " << msg->id_ << "] [text: "
                      << "Unsupported" << "]" << std::endl;
          }            
        }
      } else if (action == "download") {
        std::int64_t chat_id;
        ss >> chat_id;
        std::int32_t message_id;
        std::vector<std::int32_t> messages;
        while(ss >> message_id) {
          messages.push_back(message_id);
        }

        core_->downloadFiles(chat_id, messages);
      }

    }
  }

private:
  void console(const std::string &msg) {
    std::lock_guard<std::mutex> guard{output_lock};
    std::cout << msg << std::endl;
  }

private:
  std::unique_ptr<TdCore> core_;

};


int main() {
  {
    TdShell shell;
    shell.loop();
  }

  return 0;
}