#ifndef TDCORE_H
#define TDCORE_H

#include <functional>
#include <map>
#include <future>

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include "scopedthread.h"

namespace td_api = td::td_api;

typedef td::tl_object_ptr<td_api::chats> ChatsPtr;
typedef td::tl_object_ptr<td_api::chat> ChatPtr;
typedef td::tl_object_ptr<td_api::messages> MessagesPtr;
typedef td::tl_object_ptr<td_api::message> MessagePtr;
typedef td::tl_object_ptr<td_api::file> FilePtr;

class TdCore {

public:
  using Object = td_api::object_ptr<td_api::Object>;

public:
  TdCore();

  void start();
  void stop();
  void waitLogin();

  void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler);

  void getChats(std::promise<ChatsPtr>&, const uint32_t limit = 20);
  void getChat(std::promise<ChatPtr>&, std::int64_t chat_id);
  std::string get_chat_title(std::int64_t chat_id) const;
  std::int64_t get_chat_id(const std::string & title) const;
  std::string get_user_name(std::int64_t user_id) const;

  void getChatHistory(std::promise<MessagesPtr>&, td_api::int53 chat_id, const uint32_t limit = 20);

private:
  std::unique_ptr<td::ClientManager> client_manager_;
  std::int32_t client_id_{0};
  std::uint64_t current_query_id_{1};
  std::map<std::uint64_t, std::function<void(Object)>> handlers_;

  td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
  std::uint64_t authentication_query_id_{0};
  std::map<std::int64_t, std::string> chat_title_;
  std::map<std::int64_t, td_api::object_ptr<td_api::user>> users_;

  std::unique_ptr<ScopedThread> thread_;

  bool are_authorized_{false};
  bool need_restart_{false};
  bool stop_{false};

private:
  void loop();
  void console(const std::string &msg);

  std::uint64_t next_query_id();
  void process_response(td::ClientManager::Response response);
  void process_update(td_api::object_ptr<td_api::Object> update);
  void on_authorization_state_update();
  void check_authentication_error(Object object);

  auto create_authentication_query_handler() {
    return [this, id = authentication_query_id_](Object object) {
      if (id == authentication_query_id_) {
        check_authentication_error(std::move(object));
      }
    };
  }
};

#endif // TDCORE_H
