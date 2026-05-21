#pragma once

#include <string>

namespace accloud {

class App {
public:
  int run(int argc, char** argv);

private:
  static bool hasArg(int argc, char** argv, const std::string& flag);
};

} // namespace accloud
