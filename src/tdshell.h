#ifndef TDSHELL_H
#define TDSHELL_H

#include <iostream>
#include <sstream>
#include <cli/cli.h>

#include "tdchannel.h"
#include "common.h"
#include "commands.h"

class TdShell {

public:
  TdShell();

  void open();
  void close();
  void execute(std::string cmd, std::vector<std::string> &args, std::ostream &out);

  void cmdHistory(std::ostream& out, int64_t chat_id, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, std::string date, uint limit);
  void cmdChats(std::ostream& out);
  void cmdChatInfo(std::ostream& out, std::string chat_title);
  void cmdOpenContent(std::ostream& out, int64_t chat_id, int64_t msg_id);
  void cmdReadFile(std::ostream& out, int64_t chat_id, int64_t msg_id);

  void error(std::ostream& out, std::string msg);
  std::map<int32_t, std::string> getFileIdFromMessages(int64_t chat_id, std::vector<int64_t> msg_ids);

  MessagePtr getMessageByLink(std::string link);
  std::unique_ptr<cli::Menu> make_menu();

private:
  std::shared_ptr<TdChannel> channel_;
  std::map<std::string, std::unique_ptr<Command>> commands_;
};

#endif // TDSHELL_H