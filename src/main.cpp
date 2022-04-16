#include "tdshell.h"

#include <cli/cli.h>
#include <cli/clilocalsession.h>
#include <cli/loopscheduler.h>

using namespace cli;

std::vector<std::string> str_split(std::string str, std::string sep){
  char* cstr = const_cast<char*>(str.c_str());
  char* current;
  std::vector<std::string> arr;
  current = strtok(cstr, sep.c_str());
  while(current != NULL) {
    arr.push_back(current);
    current = strtok(NULL,sep.c_str());
  }

  return arr;
}

int main() {
  try {
    TdShell shell;
    shell.open();

    auto rootMenu = std::make_unique<Menu>("tdshell");
    rootMenu->Insert(
      "chats",
      [&shell](std::ostream& out) { shell.cmdChats(out); },
      "Get chat list.");
    rootMenu->Insert(
      "history",
      [&shell](std::ostream& out, int64_t chat_id, uint limit = 20) {
        shell.cmdHistory(out, chat_id, limit);
      },
      "Get the history of a chat.",
      {"chat_id", "limit"}
    );
    rootMenu->Insert(
      "history",
      [&shell](std::ostream& out, std::string chat_title, uint limit = 20) {
        try {
          shell.cmdHistory(out, chat_title, limit);
        } catch(const std::exception& e) {
          shell.error(out, e.what());
          shell.error(out, "Please use command 'chats' to update chat list.");
        }
      },
      "Get the history of a chat.",
      {"chat_title", "limit"}
    );
    rootMenu->Insert(
      "download",
      [&shell](std::ostream& out, int64_t chat_id, std::string messages) {
        auto msg_arr = str_split(messages, ",");
        std::vector<int32_t> msg_ids;
        for (auto &id : msg_arr) {
          msg_ids.push_back(std::stol(id));
        }
        shell.cmdDownload(out, chat_id, msg_ids);
      },
      "Download file in a message.",
      {"chat_id", "message_ids seperated by comma"}
    );

    Cli cli(std::move(rootMenu));
    SetColor();

    LoopScheduler scheduler;
    CliLocalTerminalSession localSession(cli, scheduler, std::cout, 200);
    localSession.ExitAction(
      [&scheduler, &shell](auto& out)
      {
        out << "Closing Shell...\n";
        shell.close();
        scheduler.Stop();
      }
    );

    scheduler.Run();

    return 0;

  } catch (const std::exception& e) {
      std::cerr << "Exception caugth in main: " << e.what() << std::endl;
  } catch (...) {
      std::cerr << "Unknown exception caugth in main." << std::endl;
  };

  return -1;
}