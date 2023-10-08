#ifndef TDSHELL_SESSION_H
#define TDSHELL_SESSION_H

#include <cli/cli.h>
#include <cli/clilocalsession.h>
#include <cli/loopscheduler.h>

class TDShellSession : public cli::CliSession
{
public:
    TDShellSession(cli::Cli& _cli, std::ostream& _out = std::cout, std::istream& _in = std::cin,
              std::size_t historySize = 100);

    std::istream& InStream() { return in_; }
    void Start();
    void Stop() { exit_ = true; }

private:
    //std::pair<cli::detail::Symbol, std::string> Keypressed(std::pair<cli::detail::KeyType, char> k);
    //void ResetCursor() { position_ = 0; }
    //void SetLine(const std::string &newLine);
    //std::string GetLine() const { return currentLine_; }

    //void HandleKeyboard(std::pair<cli::detail::KeyType, char> k);
    //void NewCommand(const std::pair<cli::detail::Symbol, std::string>& s);

private:
    std::istream& in_;
    bool exit_ = false;
    //std::string currentLine_;
    //std::size_t position_ = 0; // next writing position in currentLine
};

#endif // TDSHELL_SESSION_H