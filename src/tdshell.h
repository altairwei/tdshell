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

  void cmdDownload(std::ostream& out, int64_t chat_id, std::vector<int64_t> &message_ids);
  void cmdHistory(std::ostream& out, int64_t chat_id, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, std::string date, uint limit);
  void cmdChats(std::ostream& out);
  void cmdOpenContent(std::ostream& out, int64_t chat_id, int64_t msg_id);
  void cmdReadFile(std::ostream& out, int64_t chat_id, int64_t msg_id);

  void error(std::ostream& out, std::string msg);
  std::map<int32_t, std::string> getFileIdFromMessages(int64_t chat_id, std::vector<int64_t> msg_ids);

private:
  void printMessages(std::ostream& out, MessagesPtr messages);

private:
  std::unique_ptr<TdCore> core_;

};

#endif // TDSHELL_H