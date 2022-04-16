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

// overloaded
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , public overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

class TdCore {

public:
  TdCore();

  void start();
  void stop();
  void waitLogin();

  void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(ObjectPtr)> handler);

  void getChats(std::promise<ChatsPtr>&, const uint32_t limit = 20);
  void getChat(std::promise<ChatPtr>&, std::int64_t chat_id);
  std::string get_chat_title(std::int64_t chat_id) const;
  int64_t get_chat_id(const std::string & title) const;
  std::string get_user_name(std::int64_t user_id) const;

  void getChatHistory(std::promise<MessagesPtr>&, td_api::int53 chat_id, const uint32_t limit = 20);
  void downloadFiles(std::int64_t chat_id, std::vector<std::int64_t> message_ids);

  void updateChatList(int64_t id, std::string title);

private:
  std::unique_ptr<td::ClientManager> client_manager_;
  std::int32_t client_id_{0};
  std::uint64_t current_query_id_{1};
  std::map<std::uint64_t, std::function<void(ObjectPtr)>> handlers_;

  td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
  std::uint64_t authentication_query_id_{0};
  std::map<std::int64_t, std::string> chat_title_;
  std::map<std::int64_t, td_api::object_ptr<td_api::user>> users_;

  std::unique_ptr<ScopedThread> thread_;
  std::map<std::int64_t, std::string> filenames_;

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
