#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include "md2cs_config.h"

static void version(const char* progname) {
  std::cerr << progname << " version: "
            << md2cs_VERSION_MAJOR
            << "."
            << md2cs_VERSION_MINOR
            << std::endl;
  ::exit(EXIT_SUCCESS);
}

static void usage(const char* progname, int status) {
  std::cerr << "usage: " << progname
            << " ([--version|-v]|"
            << "[--help|-h]"
            << " <file.md> "
            << std::endl;
  ::exit(status);
}

int
main(int argc, char *argv[]) {

  int c;
  int digit_optind = 0;

  for (;;) {

    int this_option_optind = optind ? optind : 1;
    int option_index = 0;

    static struct option long_options[] = {
      {"version", no_argument,       0,  'v'},
      {"help",    no_argument,       0,  'h'},
      {0,         0,                 0,  0 }
    };

    c = ::getopt_long(argc, argv, "vh",
                      long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'v':
      version(argv[0]);
      break;

    case 'h':
      usage(argv[0], EXIT_SUCCESS);
      break;

    case '?':
    default:
      usage(argv[0], EXIT_FAILURE);
      break;
    }
  }

  if (optind == argc) {
    std::cerr << "No file found" << std::endl;
    usage(argv[0], EXIT_FAILURE);
  }

  std::cout << "Filename to process: "
            << argv[optind] << std::endl;

  return EXIT_SUCCESS;
}
