#ifndef COMMANDS_H
#define COMMANDS_H

#include <memory>
#include <vector>
#include <CLI/CLI.hpp>

#include "common.h"

class TdChannel;

class Program {
public:
  Program(std::string name, std::string description, std::shared_ptr<TdChannel> &channel)
    : channel_(channel), app_(std::make_unique<CLI::App>(name + " - " + description))
    , name_(name), description_(description) {}

  void parse(std::vector<std::string> args) {
    reset();
    std::reverse(args.begin(), args.end());
    app_->parse(args);
  }

  void execute(std::vector<std::string> args, std::ostream& out) {
    try {
      parse(args);
      run(out);
    } catch (const CLI::ParseError &e) {
        if(e.get_name() == "RuntimeError")
            throw;

        if(e.get_name() == "CallForHelp") {
            throw std::runtime_error(app_->help());
        }

        if(e.get_name() == "CallForAllHelp") {
            throw std::runtime_error(app_->help("", CLI::AppFormatMode::All));
        }

        if(e.get_name() == "CallForVersion") {
            throw;
        }
    }
  };

  virtual void reset() = 0;
  virtual void run(std::ostream& out) = 0;

  std::string name() { return name_; }
  std::string description() { return description_; }

protected:
  std::shared_ptr<TdChannel> channel_;
  std::unique_ptr<CLI::App> app_;
  std::string name_;
  std::string description_;
};

class CmdDownload : public Program {
public:
  CmdDownload(std::shared_ptr<TdChannel> &channel);

  void run(std::ostream& out) override;
  void reset() override;
  void downloadFileInMessages(std::ostream& out, std::vector<MessagePtr> messages);
  void download(std::ostream& out, std::string chat, std::vector<int64_t> message_ids);
  void download(std::ostream& out, int64_t chat_id, std::vector<int64_t> message_ids);
  void download(std::ostream& out, std::vector<std::string> links);
  void parseMessagesInFile(const std::string &filename);

private:
  std::vector<std::string> messages_;
  bool is_link_ = false;
  std::string chat_title_;
  std::string output_folder_;
  std::string input_file_;
};

class CmdChats : public Program {
public:
  CmdChats(std::shared_ptr<TdChannel> &channel);

  void run(std::ostream& out) override;
  void reset() override;

private:
  int32_t limit_;
  bool archive_list_;
  int32_t chat_filter_id_;
};

class CmdChatInfo : public Program {
public:
  CmdChatInfo(std::shared_ptr<TdChannel> &channel);

  void run(std::ostream& out) override;
  void reset() override;

private:
  std::string chat_;
};

class CmdHistory : public Program {
public:
  CmdHistory(std::shared_ptr<TdChannel> &channel);

  void run(std::ostream& out) override;
  void reset() override;

  void history(std::ostream& out, int64_t chat_id, int32_t limit);
  void history(std::ostream& out, std::string chat_title, int32_t limit);
  void history(std::ostream& out, std::string chat_title, std::string date, int32_t limit);

private:
  std::string chat_;
  int32_t limit_;
  std::string date_;
};

class CmdMessageLink : public Program {
public:
  CmdMessageLink(std::shared_ptr<TdChannel> &channel);

  void run(std::ostream& out) override;
  void reset() override;

private:
  std::string link_;
  std::string input_file_;
};

#endif // COMMANDS_H