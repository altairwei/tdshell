#ifndef TDSHELL_H
#define TDSHELL_H

#include <iostream>
#include <sstream>

#include "tdcore.h"
#include "common.h"

class TdShell {

public:
  TdShell();

  void open();
  void close();

  void cmdDownload(std::ostream& out, int64_t chat_id, std::vector<int32_t> &message_ids);
  void cmdHistory(std::ostream& out, int64_t chat_id, uint limit);
  void cmdHistory(std::ostream& out, std::string chat_title, uint limit);
  void cmdChats(std::ostream& out);

private:
  std::unique_ptr<TdCore> core_;

};

#endif // TDSHELL_H