#ifndef COMMON_H
#define COMMON_H

#include <mutex>

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

#endif // COMMMON_H