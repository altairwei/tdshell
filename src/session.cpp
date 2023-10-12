#include "session.h"

#include <termcolor/termcolor.hpp>

using namespace cli;
using namespace cli::detail;

TDShellSession::TDShellSession(cli::Cli& _cli, std::ostream& _out, std::istream& _in, std::size_t historySize) :
    CliSession(_cli, _out, historySize),
    in_(_in)
{
  _cli.StdExceptionHandler(
    [] (std::ostream& out, const std::string &cmd, const std::exception& e) {
      out << termcolor::colorize << termcolor::red << "Error: " << termcolor::reset << e.what() << std::endl;
    }
  );
}

void TDShellSession::Start()
{
    Enter();

    while(!exit_)
    {
      Prompt();
      std::string line;
      if (!InStream().good())
        Exit();
      std::getline(InStream(), line);
      if (InStream().eof())
        Exit();
      else
        Feed(line);
    }
}

/*
void TDShellSession::SetLine(const std::string &newLine)
{
    OutStream() << beforeInput
                << std::string(position_, '\b') << newLine
                << afterInput << std::flush;

    // if newLine is shorter than currentLine, we have
    // to clear the rest of the string
    if (newLine.size() < currentLine_.size())
    {
        OutStream() << std::string(currentLine_.size() - newLine.size(), ' ');
        // and go back
        OutStream() << std::string(currentLine_.size() - newLine.size(), '\b') << std::flush;
    }

    currentLine_ = newLine;
    position_ = currentLine_.size();
}

std::pair<Symbol, std::string> TDShellSession::Keypressed(std::pair<KeyType, char> k)
{
    switch (k.first)
    {
        case KeyType::eof:
            return std::make_pair(Symbol::eof, std::string{});
            break;
        case KeyType::backspace:
        {
            if (position_ == 0)
                break;

            --position_;

            const auto pos = static_cast<std::string::difference_type>(position_);
            // remove the char from buffer
            currentLine_.erase(currentLine_.begin() + pos);
            // go back to the previous char
            OutStream() << '\b';
            // output the rest of the line
            OutStream() << std::string(currentLine_.begin() + pos, currentLine_.end());
            // remove last char
            OutStream() << ' ';
            // go back to the original position
            OutStream() << std::string(currentLine_.size() - position_ + 1, '\b') << std::flush;
            break;
        }
        case KeyType::up:
            return std::make_pair(Symbol::up, std::string{});
            break;
        case KeyType::down:
            return std::make_pair(Symbol::down, std::string{});
            break;
        case KeyType::left:
            if (position_ > 0)
            {
                OutStream() << '\b' << std::flush;
                --position_;
            }
            break;
        case KeyType::right:
            if (position_ < currentLine_.size())
            {
                OutStream() << beforeInput
                            << currentLine_[position_]
                            << afterInput << std::flush;
                ++position_;
            }
            break;
        case KeyType::ret:
        {
            OutStream() << "\r\n";
            auto cmd = currentLine_;
            currentLine_.clear();
            position_ = 0;
            return std::make_pair(Symbol::command, cmd);
        }
        break;
        case KeyType::ascii:
        {
            const char c = static_cast<char>(k.second);
            if (c == '\t')
                return std::make_pair(Symbol::tab, std::string());
            else
            {
                const auto pos = static_cast<std::string::difference_type>(position_);

                // output the new char:
                OutStream() << beforeInput << c;
                // and the rest of the string:
                OutStream() << std::string(currentLine_.begin() + pos, currentLine_.end())
                            << afterInput;

                // go back to the original position
                OutStream() << std::string(currentLine_.size() - position_, '\b') << std::flush;

                // update the buffer and cursor position:
                currentLine_.insert(currentLine_.begin() + pos, c);
                ++position_;
            }

            break;
        }
        case KeyType::canc:
        {
            if (position_ == currentLine_.size())
                break;

            const auto pos = static_cast<std::string::difference_type>(position_);

            // output the rest of the line
            OutStream() << std::string(currentLine_.begin() + pos + 1, currentLine_.end());
            // remove last char
            OutStream() << ' ';
            // go back to the original position
            OutStream() << std::string(currentLine_.size() - position_, '\b') << std::flush;
            // remove the char from buffer
            currentLine_.erase(currentLine_.begin() + pos);
            break;
        }
        case KeyType::end:
        {
            const auto pos = static_cast<std::string::difference_type>(position_);

            OutStream() << beforeInput
                        << std::string(currentLine_.begin() + pos, currentLine_.end())
                        << afterInput << std::flush;
            position_ = currentLine_.size();
            break;
        }
        case KeyType::home:
        {
            OutStream() << std::string(position_, '\b') << std::flush;
            position_ = 0;
            break;
        }
        case KeyType::ignored:
            // TODO
            break;
    }

    return std::make_pair(Symbol::nothing, std::string());
}

void TDShellSession::HandleKeyboard(std::pair<KeyType, char> k)
{
    const std::pair<Symbol,std::string> s = Keypressed(k);
    NewCommand(s);
}

void TDShellSession::NewCommand(const std::pair<Symbol, std::string>& s)
{
    switch (s.first)
    {
        case Symbol::nothing:
        {
            break;
        }
        case Symbol::eof:
        {
            Exit();
            break;
        }
        case Symbol::command:
        {
            Feed(s.second);
            Prompt();
            break;
        }
        case Symbol::down:
        {
            SetLine(NextCmd());
            break;
        }
        case Symbol::up:
        {
            auto line = GetLine();
            SetLine(PreviousCmd(line));
            break;
        }
        case Symbol::tab:
        {
            auto line = GetLine();
            auto completions = GetCompletions(line);

            if (completions.empty())
                break;
            if (completions.size() == 1)
            {
                SetLine(completions[0]+' ');
                break;
            }

            auto commonPrefix = CommonPrefix(completions);
            if (commonPrefix.size() > line.size())
            {
                SetLine(commonPrefix);
                break;
            }
            OutStream() << '\n';
            std::string items;
            std::for_each( completions.begin(), completions.end(), [&items](auto& cmd){ items += '\t' + cmd; } );
            OutStream() << items << '\n';
            Prompt();
            ResetCursor();
            SetLine( line );
            break;
        }
    }

}

*/