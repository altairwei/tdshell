#include "tdshell.h"

#include <stdexcept>
#include <cli/cli.h>
#include <cli/clilocalsession.h>
#include <cli/loopscheduler.h>

#include "common.h"
#include "utils.h"

using namespace cli;

class MySession : public CliSession
{
public:
    MySession(Cli& _cli, Scheduler& scheduler, std::ostream& _out, std::size_t historySize = 100) :
        CliSession(_cli, _out, historySize),
        kb(scheduler),
        ih(*this, kb)
    {

    }

private:
    cli::detail::Keyboard kb;
    cli::detail::InputHandler ih;
};

int main(int argc, char* argv[]) {

  CLI::App app{"Telegram shell based on TDLib"};
  app.prefix_command();

  try {

    CLI11_PARSE(app, argc, argv);
    std::vector<std::string> arguments = app.remaining();

    TdShell shell;
    shell.open();

    Cli cli(shell.make_menu());
    SetColor();

    LoopScheduler scheduler;
    MySession localSession(cli, scheduler, std::cout, 200);
    localSession.ExitAction(
      [&scheduler, &shell](auto& out)
      {
        out << "Closing Shell..." << std::endl;
        shell.close();
        scheduler.Stop();
      }
    );

    if (arguments.empty()) {
      localSession.Prompt();
      scheduler.Run();
    } else {
      localSession.Feed(StrUtil::join(arguments, " "));
      scheduler.PollOne();
      localSession.Exit();
    }

    return 0;

  } catch (const std::exception& e) {
      std::cerr << "Exception caugth in main: " << e.what() << std::endl;
  } catch (...) {
      std::cerr << "Unknown exception caugth in main." << std::endl;
  };

  return -1;
}