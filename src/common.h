#ifndef COMMON_H
#define COMMON_H

#include <mutex>
#include <iostream>

#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

extern std::mutex output_lock;

namespace td_api = td::td_api;

typedef td::tl_object_ptr<td_api::chats> ChatListPtr;
typedef td::tl_object_ptr<td_api::chat> ChatPtr;
typedef td::tl_object_ptr<td_api::messages> MessageListPtr;
typedef td::tl_object_ptr<td_api::message> MessagePtr;
typedef td::tl_object_ptr<td_api::file> FilePtr;
typedef td_api::object_ptr<td_api::Object> ObjectPtr;
typedef td_api::object_ptr<td_api::filePart> FilePartPtr;

// overloaded
namespace FunOverload {
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
}  // namespace FunOverload

template <class... F>
auto overloaded(F... f) {
  return FunOverload::overload<F...>(f...);
}

#endif // COMMMON_H