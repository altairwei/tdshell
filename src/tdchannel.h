#ifndef TDCORE_H
#define TDCORE_H

#include <functional>
#include <map>
#include <future>

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include "scopedthread.h"
#include "common.h"

class TdChannel {

public:
  TdChannel();

  void start();
  void stop();
  void waitLogin();

  void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(ObjectPtr)> handler);

  template<typename FUN, typename RET, typename ... Args>
  void make_query(std::promise<td::tl_object_ptr<RET>> &prom, Args... args) {
    send_query(td_api::make_object<FUN>(args...),
      [this, &prom](ObjectPtr object)
      {
        if (object->get_id() == td_api::error::ID) {
          prom.set_exception(std::make_exception_ptr(
            std::logic_error("send_query failed: " + std::to_string(FUN::ID))
          ));
          return;
        }

        prom.set_value(td::move_tl_object_as<RET>(object));
      }
    );
  }

  template<typename FUN, typename ... Args>
  typename FUN::ReturnType invoke(Args... args) {
    std::promise<typename FUN::ReturnType> prom;
    make_query<FUN>(prom, args...);
    return std::move(prom.get_future().get());
  }

  //void getChats(std::promise<ChatListPtr>&, const uint32_t limit = 20);
  std::string get_chat_title(std::int64_t chat_id) const;
  int64_t get_chat_id(const std::string & title) const;
  std::string get_user_name(std::int64_t user_id) const;

  void updateChatList(int64_t id, std::string title);
  void addDownloadHandler(int32_t id, std::function<void(FilePtr)> handler);
  void removeDownloadHandler(int32_t id);
  std::function<void(FilePtr)>& getDownloadHandler(int32_t id);
  int64_t getChatId(const std::string &chat);

private:
  std::unique_ptr<td::ClientManager> client_manager_;
  std::int32_t client_id_{0};
  std::uint64_t current_query_id_{1};
  std::map<std::uint64_t, std::function<void(ObjectPtr)>> handlers_;
  std::map<std::int32_t, std::function<void(FilePtr)>> download_handlers_;

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
  void check_authentication_error(ObjectPtr object);

  auto create_authentication_query_handler() {
    return [this, id = authentication_query_id_](ObjectPtr object) {
      if (id == authentication_query_id_) {
        check_authentication_error(std::move(object));
      }
    };
  }

  void print_progress(td_api::updateFile &update_file);
};

#endif // TDCORE_H
