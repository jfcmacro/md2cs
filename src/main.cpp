#include <iostream>
#include <cstdlib>
#include "md2cs_config.h"

int
main(int argc, char *argv[]) {
  std::cout << "md2cs Version: "
            << md2cs_VERSION_MAJOR
            << "."
            << md2cs_VERSION_MINOR
            << " is not ready yet" << std::endl;
  return EXIT_SUCCESS;
}
