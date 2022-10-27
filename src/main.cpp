#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <regex>
#include <filesystem>
#include <getopt.h>
#include <git2.h>
#include "md2cs_config.h"

namespace fs = std::filesystem;

const char* readmeFilename   = "README.md";
const char* storyFileName    = "story.md";
const char* dotStoryFilename = ".story.md";
const char* targetDir        = "target";
const char* repositoriesDir  = "repositories";
const char* repositoryDir    = "repository";
const char* defaultBranch    = "main";

struct Options {
  bool debug;
  fs::path targetPath;
  Options() : debug(false), targetPath() { }
};


void processStoryFile(Options& options);
inline const char* getOutputFilename(bool);
void addBuffer2GitRepo(::git_repository* repo,
                       std::ostringstream* pBuffer,
                       const char* filename,
                       Options& options);
void commitGitRepo(::git_repository* repo,
                   std::string& message,
                   Options& options);
void m_giterror(int error,
                const char *msg,
                Options options);

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
                      "dvh",
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
  std::string currExtBranch { defaultBranch };
  std::string currExtTag;
  std::string message;

  fs::path storyDir { fs::current_path() };

  ::git_repository *repo = nullptr;

  fs::path storyFile = { fs::current_path() };
  storyFile /= storyFileName;

  if (!fs::exists(storyFile)) {
    std::cerr << "file: "
              << storyFile
              << " doesn't exists" << std::endl;
    return;
  }

  options.targetPath /= fs::current_path();
  options.targetPath /= targetDir;

  fs::path targetReposPath { fs::current_path() };
  targetReposPath /= targetDir;
  targetReposPath /= repositoriesDir;
  fs::path targetRepoPath { fs::current_path() };
  targetRepoPath /= targetDir;
  targetRepoPath /= repositoryDir;
  fs::path readMeRepoPath { fs::current_path() };
  readMeRepoPath /= targetDir;
  readMeRepoPath /= repositoryDir;

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
            }
            if (cfg[1] == "branch") {
              currExtBranch.clear();
              currExtBranch = cfg[2];
            }
            if (cfg[1] == "tag") {
              currExtTag.clear();
              currExtBranch = cfg[2];
            }
            if (cfg[1] == "focus") {
              if (pBuffer) (*pBuffer) << line << std::endl;
            }
          }
        }
        break;
      case OUTCONFIG:
        const std::regex cfg_regex("### +(.*)");
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

  ::git_libgit2_shutdown();
  std::cout << "Pages processed: " << pagesProcessed << std::endl;
  std::cout << "Commit done: " << commitDone << std::endl;
}

inline const char* getOutputFilename(bool isReadme) {
  return isReadme ? readmeFilename :
   dotStoryFilename;
}

void m_giterror(int error,
                const char *msg,
                Options options) {

  if (error < GIT_OK) {
      const ::git_error *g_error = ::git_error_last();
      std::cout << msg
                << " due ("
                << error
                << ") "
                << g_error->klass
                << " "
                << g_error->message
                << std::endl;
      if (!options.debug)
        fs::remove_all(options.targetPath);
      ::exit(EXIT_FAILURE);
    }

}

void
addBuffer2GitRepo(::git_repository* repo,
                  std::ostringstream* pBuffer,
                  const char* filename,
                  Options& options) {
  std::ofstream outputFile(filename,
                           std::ios::trunc);

  if (!outputFile) {
    std::cerr << "Could not open: "
              << filename
              << std::endl;
    ::exit(EXIT_FAILURE);
  }

  outputFile << pBuffer->str() << std::endl;
  outputFile.close();

  ::git_index *index;

  m_giterror(::git_repository_index(&index,
                                    repo),
             "Could not open repository index",
             options);

  std::string error_msg { "File: " };
  error_msg += filename;
  error_msg += " cannot be added";

  m_giterror(::git_index_add_bypath(index,
                                    filename),
             error_msg.c_str(),
             options);

  m_giterror(::git_index_write(index),
             "Index cannot be written",
             options);

  ::git_index_free(index);
}


void
commitGitRepo(::git_repository* repo,
              std::string& message,
              Options& options) {
  ::git_config *config_default;

  m_giterror(::git_config_open_default(&config_default),
             "Cannot open default configuration",
             options);

  ::git_config_entry *entry;
  m_giterror(::git_config_get_entry(&entry,
                                    config_default,
                                    "user.name"),
             "Cannot find user name at default config",
             options);
  std::string userName { entry->value };
  ::git_config_entry_free(entry);

  m_giterror(::git_config_get_entry(&entry,
                                    config_default,"user.email"),
             "Cannot find user email at default config",
             options);
  std::string userEmail { entry->value };
  ::git_config_entry_free(entry);

  ::git_signature *signature = nullptr;

  m_giterror(::git_signature_now(&signature,
                                 userName.c_str(),
                                 userEmail.c_str()),
             "Cannot create user signature",
             options);
  ::git_index *index;
  ::git_tree* tree = nullptr;
  ::git_oid tree_oid;
  ::git_reference* ref = nullptr;
  ::git_object* parent = nullptr;

  int error;
  if ((error = ::git_revparse_ext(&parent,
                                  &ref,
                                  repo,
                                  "HEAD")) != GIT_ENOTFOUND) {

    m_giterror(error,
               "Error getting parent and reference",
               options);
  }

  m_giterror(::git_repository_index(&index,
                                    repo),
             "Could not open repository index",
             options);

  m_giterror(::git_index_write_tree(&tree_oid,
                                    index),
             "Could not write tree",
             options);
  m_giterror(::git_index_write(index),
             "Could not write index",
             options);
  ::git_tree_lookup(&tree,
                    repo,
                    &tree_oid);

  ::git_oid new_commit_id;

  m_giterror(::git_commit_create_v(&new_commit_id,
                                   repo,
                                   "HEAD",
                                   signature,
                                   signature,
                                   "UTF-8",
                                   message.c_str(),
                                   tree,
                                   parent ? 1 : 0,
                                   parent),
             "Error creating commit",
             options);

  ::git_index_free(index);
  ::git_tree_free(tree);
  ::git_object_free(parent);
  ::git_reference_free(ref);
}
