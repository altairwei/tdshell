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

  void error(std::ostream& out, std::string msg);
  std::map<int32_t, std::string> getFileIdFromMessages(int64_t chat_id, std::vector<int64_t> msg_ids);

  MessagePtr getMessageByLink(std::string link);
  std::unique_ptr<cli::Menu> make_menu();

  TdChannel *channel() { return channel_.get(); }

private:
  std::shared_ptr<TdChannel> channel_;
  std::map<std::string, std::unique_ptr<Program>> commands_;
  std::unique_ptr<CLI::App> app_;
};

#endif // TDSHELL_H