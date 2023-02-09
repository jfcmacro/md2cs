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
            << "[-d] [[-p] <number-pages-process>|[--number-pages-process]]"
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
      {"number-pages-process", required_argument, 0, 'n'},
      {0,         0,                 0,  0 }
    };

    c = ::getopt_long(argc, argv,
                      "dhvn:",
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

    case 'n':
      {
        std::string n { optarg };
        options.pagesProcessed = std::stoi(n);
      }
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
  CheckoutType currCheckoutType { BRANCH };
  std::string currCheckoutName;
  std::string message;
  std::map<std::string, RepoDesc*> extRepos;

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
        if (pBuffer)
          (*pBuffer) << transTex2HTMLEntity(line) << std::endl;
        if (!currCheckoutName.empty()) {
          std::cout << (currCheckoutType == BRANCH ? "Branch" : "Tag")
                    << " to checkout: " << currCheckoutName
                    << " from repo " << currExtRepo << std::endl;
          fs::path curDir { fs::current_path() };
          fs::current_path(extRepos[currExtRepo]->repoDir);
          m_giterror(checkoutGitRepoFromName(extRepos[currExtRepo]->repo,
                                             currCheckoutName,
                                             options),
                     "Checkout failed",
                     options);

          fs::current_path(curDir);
          diffDirAction(repo,
                        extRepos[currExtRepo]->repoDir,
                        curDir,
                        options, true);
          currCheckoutName.clear();
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
            if (pBuffer)
              (*pBuffer) << transTex2HTMLEntity(line) << std::endl;

            if (firstPage) {
              firstPage = false;
            }
            else {
              commitDone++;
              commitGitRepo(repo,
                            message,
                            options);
            }

            if (pagesProcessed == options.pagesProcessed) {
              stopProcessing(pagesProcessed,
                             commitDone,
                             options);
              return;
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
              std::string currURLExtRepo;
              currURLExtRepo = cfg[2];
              fs::path curDir { fs::current_path() };
              fs::current_path(targetReposPath);
              fs::path newRepo;
              RepoDesc *rd = url2RepoDesc(currURLExtRepo);
              if (rd) {
                std::cout << "protocol: " << rd->protocol
                          << " host: " << rd->host
                          << " user: " << rd->user
                          << " repoName: " << rd->repoName
                          << std::endl;
                newRepo.clear();
                newRepo = rd->repoName;
                rd->repoDir = fs::current_path() / newRepo;
                rd->checkoutName = DEFAULTBRANCH;
                rd->checkoutType = BRANCH;
                currExtRepo = rd->repoName;
                extRepos[currExtRepo] = rd;
              }
              else {
                std::cerr << "Incorrect repo url"
                          << std::endl;
              }
              m_giterror(cloneGitRepo(newRepo,
                                      currURLExtRepo,
                                      rd,
                                      options),
                         "Clone failed",
                         options);
              fs::current_path(curDir);
            }
            if (cfg[1] == "branch") {
              currCheckoutType = BRANCH;
              currCheckoutName.clear();
              currCheckoutName = cfg[2];
            }
            if (cfg[1] == "tag") {
              currCheckoutType = TAG;
              currCheckoutName.clear();
              currCheckoutName = cfg[2];
            }
            if (cfg[1] == "focus") {
              if (pBuffer)
                (*pBuffer) << transTex2HTMLEntity(line) << std::endl;
            }
            if (cfg[1] == "origin") {
              ::git_remote *remote = nullptr;
              std::string url { cfg[2] };
              m_giterror(::git_remote_create(&remote,
                                             repo,
                                             "origin",
                                             url.c_str()),
                         "Creating remote entry",
                         options);;

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

        if (pBuffer)
          (*pBuffer) << transTex2HTMLEntity(line) << std::endl;
        break;
      }
    }
  }

  if (pBuffer)
    (*pBuffer) << transTex2HTMLEntity(line) << std::endl;

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

  stopProcessing(pagesProcessed,
                 commitDone,
                 options);
}

inline const char* getOutputFilename(bool isReadme) {
  return isReadme ? READMEFILENAME :
   DOTSTORYFILENAME;
}
