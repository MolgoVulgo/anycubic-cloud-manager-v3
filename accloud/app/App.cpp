#include "App.h"

#include <iostream>

namespace accloud {

bool App::hasArg(int argc, char** argv, const std::string& flag) {
  for (int i = 1; i < argc; ++i) {
    if (flag == argv[i]) {
      return true;
    }
  }
  return false;
}

int App::run(int argc, char** argv) {
  if (hasArg(argc, argv, "--smoke")) {
    std::cout << "accloud smoke ok" << std::endl;
    return 0;
  }

  std::cout << "accloud skeleton initialized" << std::endl;
  std::cout << "Use --smoke for CI smoke test" << std::endl;
  return 0;
}

} // namespace accloud
