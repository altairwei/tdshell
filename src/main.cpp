#include "tdshell.h"

#include <stdexcept>
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
      "chat_info",
      [&shell](std::ostream& out, std::string chat) { shell.cmdChatInfo(out, chat); },
      "Get chat list.");
    rootMenu->Insert(
      "history",
      [&shell](std::ostream& out, std::string chat_title, uint limit) {
        shell.cmdHistory(out, chat_title, limit);
      },
      "Get the history of a chat.",
      {"chat title or id", "limit"}
    );
    rootMenu->Insert(
      "history",
      [&shell](std::ostream& out, std::string chat_title, std::string date, uint limit) {
        shell.cmdHistory(out, chat_title, date, limit);
      },
      "Get the history of a chat start from given date (such as 2022-04-17).",
      {"chat title or id", "date string", "limit"}
    );
    rootMenu->Insert(
      "download",
      [&shell](std::ostream& out, std::string chat, std::string messages) {
        auto msg_arr = str_split(messages, ",");
        std::vector<int64_t> msg_ids;
        for (auto &id : msg_arr) {
          msg_ids.push_back(std::stol(id));
        }
        shell.cmdDownload(out, chat, msg_ids);
      },
      "Download file in a message.",
      {"chat_id", "message_ids seperated by comma"}
    );
    rootMenu->Insert(
      "download",
      [&shell](std::ostream& out, std::string link) {
        shell.cmdDownload(out, link);
      },
      "Download file in a message.",
      {"chat_id", "message_ids seperated by comma"}
    );
    rootMenu->Insert(
      "messagelink",
      [&shell](std::ostream& out, std::string link) {
        auto msg = shell.getMessageByLink(link);
        shell.printMessage(out, msg);
      },
      "Get message from link.",
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