#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <filesystem>
#include <getopt.h>
#include <git2.h>
#include "md2cs_config.h"
#include "helper.h"

const char* READMEFILENAME   = "README.md";
const char* STORYFILENAME    = "story.md";
const char* DOTSTORYFILENAME = ".story.md";
const char* TARGETDIR        = "target";
const char* REPOSITORIESDIR  = "repositories";
const char* REPOSITORYDIR    = "repository";
const char* DEFAULTBRANCH    = "main";
const char* STARTXMLCOMMENT  = "<!--";

inline const char* getOutputFilename(bool);

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
  std::cerr << "usage: " << std::endl
            << progname
            << "-v | --version"
            << std::endl;
  std::cerr << progname
            << "-h | --help"
            << std::endl;
  std::cerr << progname
            << "[-g]"
            << std::endl;
  ::exit(status);
}

int
main(int argc, char *argv[]) {

  int c;
  int digit_optind = 0;
  const char *progname = argv[0];
  Options options;

  for (;;) {

    int this_option_optind = optind ? optind : 1;
    int option_index = 0;

    static struct option long_options[] = {
      {"version", no_argument,       0,  'v'},
      {"help",    no_argument,       0,  'h'},
      {0,         0,                 0,  0 }
    };

    c = ::getopt_long(argc, argv,
                      "dhv",
                      long_options,
                      &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'd':
      options.debug = true;
      break;

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

  processStoryFile(options);

  return EXIT_SUCCESS;
}

void
processStoryFile(Options &options) {

  std::string currExtRepo;
  std::string currExtBranch { DEFAULTBRANCH };
  std::string currExtTag;
  std::string message;

  fs::path storyDir { fs::current_path() };

  ::git_repository *repo = nullptr;

  fs::path storyFile = { fs::current_path() /
                         STORYFILENAME };

  if (!fs::exists(storyFile)) {
    std::cerr << "file: "
              << storyFile
              << " doesn't exists" << std::endl;
    return;
  }

  options.targetPath = (fs::current_path() / TARGETDIR);

  fs::path targetReposPath { options.targetPath /
                             REPOSITORIESDIR };
  fs::path targetRepoPath { options.targetPath /
                            REPOSITORYDIR };
  fs::path readMeRepoPath { options.targetPath /
                            REPOSITORYDIR };

  m_giterror(::git_libgit2_init(),
             "Cannot initialize libgit2",
             options);

  if (fs::exists(options.targetPath)) {
    fs::remove_all(options.targetPath);
  }

  fs::create_directory(options.targetPath);
  fs::create_directory(targetReposPath);
  fs::create_directory(targetRepoPath);

  std::string error_msg { "Repo: " };
  error_msg += targetRepoPath;
  error_msg += " cannot be initialize";
  m_giterror(::git_repository_init(&repo,
                                   targetRepoPath.c_str(),
                                   false),
             error_msg.c_str(),
             options);

  ::git_index *idx = nullptr;
  m_giterror(::git_repository_index(&idx, repo),
             "Repo Index cannot be obtained",
             options);

  std::ifstream input(storyFile);

  if (!input) {
    std::cerr << "Cannot open: "
              << storyFile
              << std::endl;
    if (!options.debug) fs::remove_all(options.targetPath);
    return;
  }

  fs::current_path(targetRepoPath);

  std::cout << "Opening and processing: "
            << storyFile
            << std::endl;
  std::cout << "Working at: "
            << fs::current_path()
            << std::endl;

  std::string line;

  enum FILEPROCESS { INCONFIG, OUTCONFIG };
  FILEPROCESS state = OUTCONFIG;
  int pagesProcessed = 0;
  int commitDone = 0;
  std::ostringstream* pBuffer = new std::ostringstream();
   bool firstPage = true;

  while (std::getline(input,line)) {
    const std::regex line_regex("^(-|=){3}(-|=)* *$");
    if (std::regex_match(line,line_regex)) {
      switch(state) {
      case INCONFIG:
        state = OUTCONFIG;
        if (pBuffer) (*pBuffer) << line << std::endl;
        if (!currExtTag.empty()) {
          std::cout << "Tag to checkout: " << currExtTag << std::endl;
          m_giterror(checkoutGitRepoFromTag(repo,
                                            currExtTag,
                                            options),
                     "Checkout failed",
                     options);
          currExtTag.clear();
        }
        break;
      case OUTCONFIG:
        {
          state = INCONFIG;

          if (pBuffer->tellp() > 0) {
            pagesProcessed++;
            addBuffer2GitRepo(repo,
                              pBuffer,
                              getOutputFilename(firstPage),
                              options);
            delete pBuffer;
            pBuffer = new std::ostringstream();
            if (pBuffer) (*pBuffer) << line << std::endl;

            if (firstPage) {
              firstPage = false;
            }
            else {
              commitDone++;
              commitGitRepo(repo,
                            message,
                            options);
            }
          }
        }
        break;
      }
    }
    else {
      switch (state) {
      case INCONFIG:
        {

          const std::regex cfg_regex("(^.*): +(.*)");
          std::smatch cfg;

          if (std::regex_match(line, cfg, cfg_regex)) {
            if (cfg[1] == "repository") {
              currExtRepo.clear();
              currExtRepo = cfg[2];
              currExtBranch.clear();
              currExtBranch = "main";
              currExtTag.clear();
              fs::path curDir { fs::current_path() };
              fs::current_path(targetReposPath);
              fs::path newRepo;
              RepoDesc *rd = url2RepoDesc(currExtRepo);
              if (rd) {
                std::cout << "protocol: " << rd->protocol
                          << " host: " << rd->host
                          << " user: " << rd->user
                          << " repoName: " << rd->repoName
                          << std::endl;
                newRepo.clear();
                newRepo = rd->repoName;
                delete rd;
              }
              else {
                std::cerr << "Incorrect repo url"
                          << std::endl;
              }

              m_giterror(cloneGitRepo(newRepo,
                                      currExtRepo,
                                      options),
                         "Clone failed",
                         options);
              fs::current_path(curDir);
            }
            if (cfg[1] == "branch") {
              currExtBranch.clear();
              currExtBranch = cfg[2];
            }
            if (cfg[1] == "tag") {
              currExtTag.clear();
              currExtTag = cfg[2];
            }
            if (cfg[1] == "focus") {
              if (pBuffer) (*pBuffer) << line << std::endl;
            }
          }
        }
        break;
      case OUTCONFIG:
        const std::regex cfg_regex("^### +(.*)");
        std::smatch cfg;
        if (std::regex_match(line, cfg, cfg_regex)) {
          message.clear();
          message = cfg[1];
        }

        if (pBuffer) (*pBuffer) << line << std::endl;
        break;
      }
    }
  }

  if (pBuffer) (*pBuffer) << line << std::endl;

  pagesProcessed++;
  addBuffer2GitRepo(repo,
                    pBuffer,
                    getOutputFilename(firstPage),
                    options);

  delete pBuffer;
  if (firstPage) {
    firstPage = false;
  }
  else {
    commitDone++;
    commitGitRepo(repo,
                  message,
                  options);
  }

  int error;
  while ((error = ::git_libgit2_shutdown()) != 0) {
    if (error < GIT_OK)
      m_giterror(error, "Libgit2 shutdown has failed", options);
  }

  std::cout << "Pages processed: " << pagesProcessed << std::endl;
  std::cout << "Commit done: " << commitDone << std::endl;
}

inline const char* getOutputFilename(bool isReadme) {
  return isReadme ? READMEFILENAME :
   DOTSTORYFILENAME;
}
