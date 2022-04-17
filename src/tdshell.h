#ifndef TDSHELL_H
#define TDSHELL_H

#include <iostream>
#include <sstream>

#include "tdcore.h"
#include "common.h"

class TdShell {

public:
  TdShell();

  void open();
  void close();

  void cmdDownload(std::ostream& out, std::string chat, std::vector<int64_t> message_ids);
  void cmdDownload(std::ostream& out, int64_t chat_id, std::vector<int64_t> message_ids);
  void cmdDownload(std::ostream& out, std::string link);
  void cmdHistory(std::ostream& out, int64_t chat_id, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, std::string date, uint limit);
  void cmdChats(std::ostream& out);
  void cmdChatInfo(std::ostream& out, std::string chat_title);
  void cmdOpenContent(std::ostream& out, int64_t chat_id, int64_t msg_id);
  void cmdReadFile(std::ostream& out, int64_t chat_id, int64_t msg_id);

  void error(std::ostream& out, std::string msg);
  std::map<int32_t, std::string> getFileIdFromMessages(int64_t chat_id, std::vector<int64_t> msg_ids);

  int64_t getChatId(std::string chat);
  void printMessage(std::ostream& out, MessagePtr &message);
  MessagePtr getMessageByLink(std::string link);

private:
  std::unique_ptr<TdCore> core_;

};

#endif // TDSHELL_H