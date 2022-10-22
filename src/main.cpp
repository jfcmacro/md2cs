#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <regex>
#include <filesystem>
#include <getopt.h>
#include <git2.h>
#include "md2cs_config.h"

void processDir();

static void version(const char* progname) {
  std::cerr << progname << " version: "
            << md2cs_VERSION_MAJOR
            << "."
            << md2cs_VERSION_MINOR
            << std::endl;
  ::exit(EXIT_SUCCESS);
}

static void usage(const char* progname,
                  int status) {
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
  const char *progname = argv[0];

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
      version(progname);
      break;

    case 'h':
      usage(progname, EXIT_SUCCESS);
      break;

    case '?':
    default:
      usage(progname, EXIT_FAILURE);
      break;
    }
  }

  processDir();

  return EXIT_SUCCESS;
}

void
processDir() {

  namespace fs = std::filesystem;
  fs::path storyDir { fs::current_path() };
  ::git_repository *repo = nullptr;

  std::cout << "Starting" << std::endl;

  fs::path readmeFile { fs::current_path() };
  readmeFile /= "README.md";
  fs::path storyFile = { fs::current_path() };
  storyFile /= "story.md";

  if (!fs::exists(readmeFile)) {
    std::cerr << "file: "
              << readmeFile
              << " doesn't exists" << std::endl;
    return;
  }

  if (!fs::exists(storyFile)) {
    std::cerr << "file: "
              << storyFile
              << " doesn't exists" << std::endl;
    return;
  }

  fs::path targetPath { fs::current_path() };
  targetPath /= "target";
  fs::path targetReposPath { fs::current_path() };
  targetReposPath /= "target";
  targetReposPath /= "repositories";
  fs::path targetRepoPath { fs::current_path() };
  targetRepoPath /= "target";
  targetRepoPath /= "repository";
  fs::path readMeRepoPath { fs::current_path() };
  readMeRepoPath /= "target";
  readMeRepoPath /= "repository";

  ::git_libgit2_init();

  if (fs::exists(targetPath)) {
    fs::remove_all(targetPath);
  }

  fs::create_directory(targetPath);
  fs::create_directory(targetReposPath);
  fs::create_directory(targetRepoPath);

  int error = git_repository_init(&repo,
                                  targetRepoPath.c_str(),
                                  false);
  if (error < GIT_OK) {
    const ::git_error *g_error = ::git_error_last();
    std::cout << "Repo: "
              << targetRepoPath
              << " cannot be initialize due ("
              << error
              << ") "
              << g_error->klass
              << " "
              << g_error->message
              << std::endl;
    // fs::remove_all(targetPath);
    return;
  }

  ::git_index *idx = nullptr;
  error = ::git_repository_index(&idx, repo);

  if (error < GIT_OK) {
    const ::git_error *g_error = ::git_error_last();
    std::cout << "Repo Index cannot be obtained due ("
              << error
              << ") "
              << g_error->klass
              << " "
              << g_error->message
              << std::endl;
    // fs::remove_all(targetPath);
    return;
  }

  fs::copy(readmeFile, readMeRepoPath);

  fs::current_path(targetRepoPath);

  error = ::git_index_add_bypath(idx, "README.md");

  if (error < GIT_OK) {
    const ::git_error *g_error = ::git_error_last();
    std::cout << "File "
              << readMeRepoPath
              << " cannot be added due ("
              << error
              << ") "
              << g_error->klass
              << " "
              << g_error->message
    << std::endl;
    // fs::remove_all(targetPath);
    return;
  }

  
  error = ::git_index_write(idx);

  if (error < GIT_OK) {
    const ::git_error *g_error = ::git_error_last();
    std::cout << "Index cannot be written due ("
              << error
              << ") "
              << g_error->klass
              << " "
              << g_error->message
    << std::endl;
    // fs::remove_all(targetPath);
    return;
  }


  fs::current_path(storyDir);

  std::cout << "Opening: "
            << storyFile
            << std::endl;

  std::ifstream input(storyFile);

  std::cout << "Processing file: "
            << storyFile
            << std::endl;

  std::string line;
  const std::regex line_regex("^(-|=)+$");

  enum FILEPROCESS { INCONFIG, OUTCONFIG };
  FILEPROCESS state = OUTCONFIG;
  int pagesProcessed = 0;

  while (std::getline(input,line)) {
    if (std::regex_match(line,line_regex)) {
      switch(state) {
      case INCONFIG:
        state = OUTCONFIG;
        break;
      case OUTCONFIG:
        state = INCONFIG;
        pagesProcessed++;
        break;
      }
    }
    else {
      switch (state) {
      case INCONFIG: {
        const std::regex cfg_regex("(^.*): +(.*)");
        std::smatch cfg;
        if (std::regex_match(line, cfg, cfg_regex)) {
        }
        break;
      }
      case OUTCONFIG:
        break;
      }
    }
  }

  ::git_libgit2_shutdown();
  std::cout << "Pages processed: " << pagesProcessed << std::endl;
}
