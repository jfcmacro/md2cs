#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include <map>
#include <git2.h>

namespace fs = std::filesystem;

struct Options {
  bool upload;
  bool debug;
  int pagesProcessed;
  fs::path targetPath;
  Options() : upload(false), debug(false), targetPath(), pagesProcessed(-1) { }
};

enum CheckoutType { BRANCH, TAG };

struct RepoDesc {
  std::string protocol;
  std::string host;
  std::string user;
  std::string repoName;
  fs::path repoDir;
  CheckoutType checkoutType;
  std::string checkoutName;
  ::git_repository *repo;
  RepoDesc(std::string protocol,
           std::string host,
           std::string user,
           std::string repoName) :
    protocol(protocol),
    host(host),
    user(user),
    repoName(repoName),
    repoDir(""),
    checkoutType(BRANCH),
    checkoutName("main"),
    repo(nullptr)
    { }
};

void processStoryFile(Options& options);
std::string transTex2HTMLEntity(const std::string& input);
void addBuffer2GitRepo(::git_repository* repo,
                       std::ostringstream* pBuffer,
                       const char* filename,
                       Options& options);
void add2GitRepo(::git_repository *repo,
                 const fs::path& filePath,
                 Options& options);
void addFile2GitRepo(::git_repository *repo,
                     const fs::path& filePath,
                     Options& options);
void addFile2GitRepo(::git_repository *repo,
                     const char* filename,
                     Options& options);
void addPath2GitRepo(::git_repository *repo,
                     const fs::path& filePath,
                     Options& options);
void removePath2GitRepo(::git_repository *repo,
                        const fs::path& filePath,
                        Options& options);
void removeDir2GitRepo(::git_repository* repo,
                       const char* dirName,
                       Options& options);
void moveFile2GitRepo(::git_repository *repo,
                      const fs::path& srcPath,
                      const fs::path& dstPath,
                      Options& options);
void commitGitRepo(::git_repository* repo,
                   std::string& message,
                   Options& options);
void m_giterror(int error,
                const char *msg,
                Options options);
int cloneGitRepo(fs::path& location,
                 std::string& url,
                 RepoDesc* rd,
                 Options& options);
int checkoutGitRepoFromName(::git_repository* repo,
                            const std::string& tag,
                            Options& options);
int pushGitRepo(::git_repository* repo,
                Options& options,
                const char* refSpec,
                bool force = false);
RepoDesc* url2RepoDesc(std::string& url);
void diffDirAction(::git_repository* repo,
                   fs::path srcDir,
                   fs::path dstDir,
                   Options& options,
                   bool isRoot = false);
void stopProcessing(int pagesProcessed,
                    int commitDone,
                    Options& options);
::git_repository* initLocalRepository(fs::path& repoPath,
                                      Options& options);
