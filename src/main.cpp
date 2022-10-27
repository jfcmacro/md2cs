#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <regex>
#include <list>
#include <filesystem>
#include <getopt.h>
#include <git2.h>
#include "md2cs_config.h"

const char* readmeFilename   = "README.md";
const char* storyFileName    = "story.md";
const char* dotStoryFilename = ".story.md";
const char* targetDir        = "target";
const char* repositoriesDir  = "repositories";
const char* repositoryDir    = "repository";
const char* defaultBranch    = "main";

void processStoryFile();
inline const char* getOutputFilename(bool);
void m_giterror(int error,
                const char *msg);

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

    c = ::getopt_long(argc, argv,
                      "vh",
                      long_options,
                      &option_index);
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

  processStoryFile();

  return EXIT_SUCCESS;
}

void
processStoryFile() {

  std::string currExtRepo;
  std::string currExtBranch { defaultBranch };
  std::string currExtTag;
  std::string message;

  namespace fs = std::filesystem;

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

  fs::path targetPath { fs::current_path() };
  targetPath /= targetDir;
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
             "Cannot initialize libgit2");

  if (fs::exists(targetPath)) {
    fs::remove_all(targetPath);
  }

  fs::create_directory(targetPath);
  fs::create_directory(targetReposPath);
  fs::create_directory(targetRepoPath);

  ::git_config *config_default;

  m_giterror(::git_config_open_default(&config_default),
             "Cannot open default configuration");

  ::git_config_entry *entry;
  m_giterror(::git_config_get_entry(&entry, config_default, "user.name"),
             "Cannot find user name at default config");
  std::string userName { entry->value };
  ::git_config_entry_free(entry);

  m_giterror(::git_config_get_entry(&entry, config_default, "user.email"),
             "Cannot find user email at default config");
  std::string userEmail { entry->value };
  ::git_config_entry_free(entry);

  ::git_signature *signature = nullptr;

  m_giterror(::git_signature_now(&signature,
                                 userName.c_str(),
                                 userEmail.c_str()),
             "Cannot create user signature");

  std::string error_msg { "Repo: " };
  error_msg += targetRepoPath;
  error_msg += " cannot be initialize";
  m_giterror(::git_repository_init(&repo,
                                   targetRepoPath.c_str(),
                                   false),
             error_msg.c_str());

  std::list<git_commit*> commit_parents;

  ::git_index *idx = nullptr;
  m_giterror(::git_repository_index(&idx, repo),
             "Repo Index cannot be obtained");

  std::ifstream input(storyFile);

  if (!input) {
    std::cerr << "Cannot open: "
              << storyFile
              << std::endl;
    // fs::remove_all(targetPath);
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
            std::ofstream outputFile(getOutputFilename(firstPage),
                                     std::ios::trunc);
            outputFile << pBuffer->str() << std::endl;
            outputFile.close();
            delete pBuffer;

            error_msg.clear();
            error_msg += "File: ";
            error_msg += getOutputFilename(firstPage);
            error_msg += " cannot be added";

            m_giterror(::git_index_add_bypath(idx,
                                              getOutputFilename(firstPage)),
                       error_msg.c_str());

            m_giterror(::git_index_write(idx),
                       "Index cannot be written");

            pBuffer = new std::ostringstream();
            if (pBuffer) (*pBuffer) << line << std::endl;

            if (firstPage) {
              firstPage = false;
            }
            else {
              ::git_tree* tree = nullptr;
              ::git_oid* oid_tree;
              ::git_index_write_tree(oid_tree, idx);
              ::git_tree_lookup(&tree, repo, oid_tree);

              const ::git_commit* parents[1];
              int nParents;

              if (commit_parents.size() == 0) {
                parents[0] = nullptr;
                nParents = 0;
              }
              else {
                parents[0] = commit_parents.back();
                nParents = 1;
              }

              ::git_oid new_commit_id;

              m_giterror(::git_commit_create(&new_commit_id,
                                             repo,
                                             "HEAD",
                                             signature,
                                             signature,
                                             "UTF-8",
                                             message.c_str(),
                                             tree,
                                             1,
                                             parents),
                         "Commit cannot be done");

              ::git_commit *previous_commit;
              m_giterror(::git_commit_lookup(&previous_commit,
                                             repo,
                                             &new_commit_id),
                         "Previous commit not found");

              commit_parents.push_back(previous_commit);
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

  std::ofstream outputFile(getOutputFilename(firstPage),
                           std::ios::trunc);
  outputFile << pBuffer->str() << std::endl;
  outputFile.close();
  delete pBuffer;
  pBuffer = new std::ostringstream();

  error_msg.clear();
  error_msg += "File: ";
  error_msg += getOutputFilename(firstPage);
  error_msg += " cannot be added";
  
  m_giterror(::git_index_add_bypath(idx,
                                    getOutputFilename(firstPage)),
             error_msg.c_str());

  m_giterror(::git_index_write(idx),
             "Index cannot be written");

  if (firstPage) {
    firstPage = false;
  }
  else {

    ::git_tree* tree = nullptr;
    ::git_oid* oid_tree;
    ::git_index_write_tree(oid_tree, idx);
    ::git_tree_lookup(&tree, repo, oid_tree);

    const ::git_commit* parents[1];
    int nParents;

    if (commit_parents.size() == 0) {
      parents[0] = nullptr;
      nParents = 0;
    }
    else {
      parents[0] = commit_parents.back();
      nParents = 1;
    }

    ::git_oid new_commit_id;

    m_giterror(::git_commit_create(&new_commit_id,
                                   repo,
                                   "HEAD",
                                   signature,
                                   signature,
                                   "UTF-8",
                                   message.c_str(),
                                   tree,
                                   1,
                                   parents),
               "Commit cannot be done");

    ::git_commit *previous_commit;
    ::git_commit_lookup(&previous_commit, repo, &new_commit_id);

    commit_parents.push_back(previous_commit);
  }

  ::git_libgit2_shutdown();
  std::cout << "Pages processed: " << pagesProcessed << std::endl;
}

inline const char* getOutputFilename(bool isReadme) {
  return isReadme ? readmeFilename :
   dotStoryFilename;
}

void m_giterror(int error,
                const char *msg) {
  if (error < GIT_OK) {
      const ::git_error *g_error = ::git_error_last();
      std::cout << msg
                << " due ("
                << ") "
                << g_error->klass
                << " "
                << g_error->message
                << std::endl;
      // fs::remove_all(targetPath);
      ::exit(EXIT_FAILURE);
    }

}
