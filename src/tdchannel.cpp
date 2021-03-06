#include "tdchannel.h"

#include <limits>
#include <iostream>
#include <mutex>
#include <algorithm>

std::mutex output_lock;

TdChannel::TdChannel() {
  td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
  client_manager_ = std::make_unique<td::ClientManager>();
  client_id_ = client_manager_->create_client_id();
  send_query(td_api::make_object<td_api::getOption>("version"), {});
}

void TdChannel::start() {
  thread_ = std::make_unique<ScopedThread>([this] {
    this->loop();
  });
}

void TdChannel::stop() {
  stop_ = true;
}

void TdChannel::loop() {
  while(true) {
    if (stop_) return;
    auto response = client_manager_->receive(5);
    if (response.object) {
      process_response(std::move(response));
    }
  }
}

void TdChannel::waitLogin()
{
  while(!are_authorized_)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

}

void TdChannel::send_query(td_api::object_ptr<td_api::Function> f, std::function<void(ObjectPtr)> handler) {
  auto query_id = next_query_id();
  if (handler) {
    handlers_.emplace(query_id, std::move(handler));
  }
  client_manager_->send(client_id_, query_id, std::move(f));
}

std::uint64_t TdChannel::next_query_id() {
  if (current_query_id_ == std::numeric_limits<uint64_t>::max())
    current_query_id_ = 1;
  
  return current_query_id_++;
}

void TdChannel::process_response(td::ClientManager::Response response) {
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

void TdChannel::process_update(td_api::object_ptr<td_api::Object> update) {
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
                      auto &handler = getDownloadHandler(update_file.file_->id_);
                      handler(std::move(update_file.file_));
                    },
                    [](auto &update) {}));
}

void TdChannel::on_authorization_state_update() {
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

void TdChannel::check_authentication_error(ObjectPtr object) {
  if (object->get_id() == td_api::error::ID) {
    auto error = td::move_tl_object_as<td_api::error>(object);
    std::cout << "Error: " << to_string(error) << std::flush;
    on_authorization_state_update();
  }
}

void TdChannel::console(const std::string &msg) {
  std::lock_guard<std::mutex> guard{output_lock};
  std::cout << msg << std::endl;
}

std::string TdChannel::get_user_name(std::int64_t user_id) const {
  auto it = users_.find(user_id);
  if (it == users_.end()) {
    return "unknown user";
  }
  return it->second->first_name_ + " " + it->second->last_name_;
}

std::string TdChannel::get_chat_title(std::int64_t chat_id) const {
  auto it = chat_title_.find(chat_id);
  if (it == chat_title_.end()) {
    return "unknown chat";
  }
  return it->second;
}

int64_t TdChannel::get_chat_id(const std::string & title) const
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

void TdChannel::updateChatList(int64_t id, std::string title) {
  chat_title_[id] = title;
}

void TdChannel::addDownloadHandler(int32_t id, std::function<void(FilePtr)> handler) {
  download_handlers_.emplace(id, std::move(handler));
}

void TdChannel::removeDownloadHandler(int32_t id) {
  download_handlers_.erase(id);
}

std::function<void(FilePtr)>& TdChannel::getDownloadHandler(int32_t id) {
  return download_handlers_[id];
}

int64_t TdChannel::getChatId(const std::string &chat) {
  int64_t chat_id;

  try {
    chat_id = std::stoll(chat);
  } catch (std::invalid_argument const& e) {
    chat_id = get_chat_id(chat);
    if (chat_id == 0)
      throw std::logic_error(
        "Not found chat " + chat +
        ". Please use command 'chats' to update chat list.");
  }

  return chat_id;
}