#include "tdshell.h"

#include <stdexcept>
#include <cli/cli.h>
#include <cli/clilocalsession.h>
#include <cli/loopscheduler.h>

#include "common.h"

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

    Cli cli(shell.make_menu());
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