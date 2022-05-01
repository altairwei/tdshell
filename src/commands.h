#ifndef COMMANDS_H
#define COMMANDS_H

#include <memory>
#include <vector>
#include <CLI/CLI.hpp>

#include "common.h"

class TdChannel;

class Command {
public:
  Command(std::string name, std::string description, std::shared_ptr<TdChannel> &channel)
    : channel_(channel), app_(std::make_unique<CLI::App>(description)), name_(name) {}

  virtual void parse(std::vector<std::string> args) {
    reset();
    std::reverse(args.begin(), args.end());
    app_->parse(args);
  }

  virtual void run(std::vector<std::string> args, std::ostream& out) = 0;
  virtual void reset() = 0;

protected:
  std::shared_ptr<TdChannel> channel_;
  std::unique_ptr<CLI::App> app_;
  std::string name_;
};

class CmdDownload : public Command {
public:
  CmdDownload(std::shared_ptr<TdChannel> &channel);

  void run(std::vector<std::string> args, std::ostream& out) override;
  void reset() override;
  void downloadFileInMessages(std::ostream& out, std::vector<MessagePtr> messages);
  void download(std::ostream& out, std::string chat, std::vector<int64_t> message_ids);
  void download(std::ostream& out, int64_t chat_id, std::vector<int64_t> message_ids);
  void download(std::ostream& out, std::vector<std::string> links);

private:
  std::vector<std::string> messages_;
  bool is_link_ = false;
  std::string chat_title_;
};

#endif // COMMANDS_H