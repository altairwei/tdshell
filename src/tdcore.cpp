#include "tdcore.h"

#include <limits>
#include <iostream>
#include <mutex>
#include <algorithm>

#include "common.h"

std::mutex output_lock;

TdCore::TdCore() {
  td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
  client_manager_ = std::make_unique<td::ClientManager>();
  client_id_ = client_manager_->create_client_id();
  send_query(td_api::make_object<td_api::getOption>("version"), {});
}

void TdCore::start() {
  thread_ = std::make_unique<ScopedThread>([this] {
    this->loop();
  });
}

void TdCore::stop() {
  stop_ = true;
}

void TdCore::loop() {
  while(true) {
    if (stop_) return;
    auto response = client_manager_->receive(5);
    if (response.object) {
      process_response(std::move(response));
    }
  }
}

void TdCore::waitLogin()
{
  while(!are_authorized_)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

}

void TdCore::send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
  auto query_id = next_query_id();
  if (handler) {
    handlers_.emplace(query_id, std::move(handler));
  }
  client_manager_->send(client_id_, query_id, std::move(f));
}

std::uint64_t TdCore::next_query_id() {
  if (current_query_id_ == std::numeric_limits<uint64_t>::max())
    current_query_id_ = 1;
  
  return current_query_id_++;
}

void TdCore::process_response(td::ClientManager::Response response) {
  if (!response.object) {
    return;
  }

  if (response.request_id == 0) {
    return process_update(std::move(response.object));
  }

  auto it = handlers_.find(response.request_id);
  if (it != handlers_.end()) {
    it->second(std::move(response.object));
    handlers_.erase(it);
  }
}

void TdCore::process_update(td_api::object_ptr<td_api::Object> update) {
  td_api::downcast_call(
      *update, overloaded(
                    [this](td_api::updateAuthorizationState &update_authorization_state) {
                      authorization_state_ = std::move(update_authorization_state.authorization_state_);
                      on_authorization_state_update();
                    },
                    [this](td_api::updateNewChat &update_new_chat) {
                      chat_title_[update_new_chat.chat_->id_] = update_new_chat.chat_->title_;
                    },
                    [this](td_api::updateChatTitle &update_chat_title) {
                      chat_title_[update_chat_title.chat_id_] = update_chat_title.title_;
                    },
                    [this](td_api::updateUser &update_user) {
                      auto user_id = update_user.user_->id_;
                      users_[user_id] = std::move(update_user.user_);
                    },
                    [this](td_api::updateNewMessage &update_new_message) {
                      auto chat_id = update_new_message.message_->chat_id_;
                      std::string sender_name;
                      td_api::downcast_call(*update_new_message.message_->sender_id_,
                                            overloaded(
                                                [this, &sender_name](td_api::messageSenderUser &user) {
                                                  sender_name = get_user_name(user.user_id_);
                                                },
                                                [this, &sender_name](td_api::messageSenderChat &chat) {
                                                  sender_name = get_chat_title(chat.chat_id_);
                                                }));
                      std::string text;
                      if (update_new_message.message_->content_->get_id() == td_api::messageText::ID) {
                        text = static_cast<td_api::messageText &>(*update_new_message.message_->content_).text_->text_;
                      }
                      //std::cout << "Got message: [chat_id:" << chat_id << "] [from:" << sender_name << "] [" << text
                      //          << "]" << std::endl;
                    },
                    [this](td_api::updateFile &update_file) {
                      std::lock_guard<std::mutex> guard{output_lock};
                      print_progress(update_file);
                    },
                    [](auto &update) {}));
}

void TdCore::on_authorization_state_update() {
  authentication_query_id_++;
  td_api::downcast_call(
      *authorization_state_,
      overloaded(
          [this](td_api::authorizationStateReady &) {
            are_authorized_ = true;
            console("Got authorization");
          },
          [this](td_api::authorizationStateLoggingOut &) {
            are_authorized_ = false;
            console("Logging out");
          },
          [this](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
          [this](td_api::authorizationStateClosed &) {
            are_authorized_ = false;
            need_restart_ = true;
            console("Terminated");
          },
          [this](td_api::authorizationStateWaitCode &) {
            std::cout << "Enter authentication code: " << std::flush;
            std::string code;
            std::cin >> code;
            send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                        create_authentication_query_handler());
          },
          [this](td_api::authorizationStateWaitRegistration &) {
            std::lock_guard<std::mutex> guard{output_lock};
            std::string first_name;
            std::string last_name;
            std::cout << "Enter your first name: " << std::flush;
            std::cin >> first_name;
            std::cout << "Enter your last name: " << std::flush;
            std::cin >> last_name;
            send_query(td_api::make_object<td_api::registerUser>(first_name, last_name),
                        create_authentication_query_handler());
          },
          [this](td_api::authorizationStateWaitPassword &) {
            std::lock_guard<std::mutex> guard{output_lock};
            std::cout << "Enter authentication password: " << std::flush;
            std::string password;
            std::getline(std::cin, password);
            send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                        create_authentication_query_handler());
          },
          [this](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
            console("Confirm this login link on another device: " + state.link_);
          },
          [this](td_api::authorizationStateWaitPhoneNumber &) {
            std::lock_guard<std::mutex> guard{output_lock};
            std::cout << "Enter phone number: " << std::flush;
            std::string phone_number;
            std::cin >> phone_number;
            send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                        create_authentication_query_handler());
          },
          [this](td_api::authorizationStateWaitEncryptionKey &) {
            std::lock_guard<std::mutex> guard{output_lock};
            std::cout << "Enter encryption key or DESTROY: " << std::flush;
            std::string key;
            std::getline(std::cin, key);
            if (key == "DESTROY") {
              send_query(td_api::make_object<td_api::destroy>(), create_authentication_query_handler());
            } else {
              send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(std::move(key)),
                          create_authentication_query_handler());
            }
          },
          [this](td_api::authorizationStateWaitTdlibParameters &) {
            auto parameters = td_api::make_object<td_api::tdlibParameters>();
            parameters->database_directory_ = "tdlib";
            parameters->use_message_database_ = true;
            parameters->use_secret_chats_ = true;
            parameters->api_id_ = 94575;
            parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
            parameters->system_language_code_ = "en";
            parameters->device_model_ = "Desktop";
            parameters->application_version_ = "1.0";
            parameters->enable_storage_optimizer_ = true;
            send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                        create_authentication_query_handler());
          }));
}

void TdCore::check_authentication_error(Object object) {
  if (object->get_id() == td_api::error::ID) {
    auto error = td::move_tl_object_as<td_api::error>(object);
    std::cout << "Error: " << to_string(error) << std::flush;
    on_authorization_state_update();
  }
}

void TdCore::console(const std::string &msg) {
  std::lock_guard<std::mutex> guard{output_lock};
  std::cout << msg << std::endl;
}

std::string TdCore::get_user_name(std::int64_t user_id) const {
  auto it = users_.find(user_id);
  if (it == users_.end()) {
    return "unknown user";
  }
  return it->second->first_name_ + " " + it->second->last_name_;
}

std::string TdCore::get_chat_title(std::int64_t chat_id) const {
  auto it = chat_title_.find(chat_id);
  if (it == chat_title_.end()) {
    return "unknown chat";
  }
  return it->second;
}

std::int64_t TdCore::get_chat_id(const std::string & title) const
{
  auto result = std::find_if(
    chat_title_.cbegin(),
    chat_title_.cend(),
    [&title] (const auto& p) { return p.second == title; }
  );

  if (result != chat_title_.cend())
    return result->first;
  else
    return 0;
}

void TdCore::getChats(std::promise<ChatsPtr> &prom, const uint32_t limit) {
  send_query(td_api::make_object<td_api::getChats>(nullptr, limit), [this, &prom](Object object) {
    if (object->get_id() == td_api::error::ID) {
      prom.set_exception(std::make_exception_ptr(std::logic_error("getChats failed")));
      return;
    }

    prom.set_value(td::move_tl_object_as<td_api::chats>(object));
  });
}

void TdCore::getChat(std::promise<ChatPtr>& prom, std::int64_t chat_id) {
  send_query(td_api::make_object<td_api::getChat>(chat_id), [this, &prom](Object object) {
    if (object->get_id() == td_api::error::ID) {
      prom.set_exception(std::make_exception_ptr(std::logic_error("getChat failed")));
      return;
    }

    prom.set_value(td::move_tl_object_as<td_api::chat>(object));
  });
}

void TdCore::getChatHistory(std::promise<MessagesPtr>& prom, td_api::int53 chat_id, const uint32_t limit) {
  std::promise<ChatPtr> chat_prom;
  auto chat_fut = chat_prom.get_future();
  getChat(chat_prom, chat_id);
  auto chat = chat_fut.get();

  send_query(
    td_api::make_object<td_api::getChatHistory>(
      chat_id, chat->last_message_->id_, 0, limit, false),
    [this, &prom](Object object) {
      if (object->get_id() == td_api::error::ID) {
        prom.set_exception(std::make_exception_ptr(std::logic_error("getChatHistory failed")));
        return;
      }

      prom.set_value(td::move_tl_object_as<td_api::messages>(object));
    }
  );
}

void TdCore::downloadFiles(std::int64_t chat_id, std::vector<std::int32_t> message_ids) {
  std::vector<MessagePtr> MsgObjs;
  for (auto msg_id : message_ids) {
    std::promise<MessagePtr> prom;
    auto fut = prom.get_future();
    send_query(
      td_api::make_object<td_api::getMessage>(chat_id, msg_id),
      [this, &prom](TdCore::Object object) {
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

    filenames_.insert({file_id, filename});

    send_query(
      td_api::make_object<td_api::downloadFile>(file_id, 32, 0, 0, true),
      [this, &prom](TdCore::Object object) {
        if (object->get_id() == td_api::error::ID) {
          prom.set_exception(std::make_exception_ptr(std::logic_error("downloadFile failed")));
          return;
        }

        prom.set_value(td::move_tl_object_as<td_api::file>(object));
      }
    );
  }

  for (auto &prom : promises) {
    auto file = prom.get_future().get();
  }

  filenames_.clear();
}

static inline void move_up(int lines) { std::cout << "\033[" << lines << "A"; }

void TdCore::print_progress(td_api::updateFile &update_file) {
  double total = update_file.file_->expected_size_;
  double down = update_file.file_->local_->downloaded_size_;
  double progress = down / total;

  std::string filename = filenames_[update_file.file_->id_];
  int barWidth = 50;
  std::cout << filename;
  std::cout << " [";
  int pos = barWidth * progress;
  for (int i = 0; i < barWidth; ++i) {
      if (i < pos) std::cout << "=";
      else if (i == pos) std::cout << ">";
      else std::cout << " ";
  }
  std::cout << "] " << int(progress * 100.0) << " %\r";

  if (update_file.file_->local_->is_downloading_completed_) {
    std::cout << std::endl;
    std::cout << update_file.file_->local_->path_ << std::endl;
  }

  std::cout.flush();
}