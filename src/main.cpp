#include "tdshell.h"

#include <stdexcept>
#include <nowide/iostream.hpp>
#include <nowide/args.hpp>

#include "common.h"
#include "utils.h"
#include "session.h"

using namespace cli;

int main(int argc, char* argv[]) {
  nowide::args _(argc,argv); // Fix arguments - make them UTF-8

  CLI::App app{"Telegram shell based on TDLib"};

  bool empty_key = false;
  bool new_key = false;
  app.add_flag("-N,--empty-key", empty_key, "Use empty string as the encryption key for the local database.");
  app.add_flag("--new-encryption-key", new_key, "Set a new encryption key.");

  std::string database_path("tdlib");
  app.add_option("-d,--database", database_path, "The path to the directory on the local disk "
    "where the TDLib database is to be stored; must point to a writable directory.")
    ->capture_default_str();

  app.prefix_command();

  try {

    CLI11_PARSE(app, argc, argv);

    std::vector<std::string> arguments = app.remaining();
    bool interactive = arguments.empty();

    TdShell shell;
    shell.channel()->useEmptyEncryptionKey(empty_key);
    shell.channel()->setDatabaseDirectory(database_path);
    shell.open();

    if (new_key) {
      std::string new_password = ConsoleUtil::getPassword("Enter a new encryption key: ");
      shell.channel()->invoke<td_api::setDatabaseEncryptionKey>(new_password);
    }

    Cli cli(shell.make_menu());
    SetColor();

    TDShellSession localSession(cli, nowide::cout, nowide::cin, 200);
    localSession.ExitAction(
      [&localSession, &shell, &interactive](auto& out)
      {
        if (interactive)
          out << "Closing Shell..." << std::endl;
        shell.close();
        localSession.Stop();
      }
    );

    if (interactive) {
      localSession.Start();
    } else {
      localSession.Feed(StrUtil::join(arguments, " "));
      localSession.Exit();
      localSession.Stop();
    }

    return 0;

  } catch (const std::exception& e) {
      std::cerr << "Exception caught in main: " << e.what() << std::endl;
  } catch (...) {
      std::cerr << "Unknown exception caught in main." << std::endl;
  };

  return -1;
}