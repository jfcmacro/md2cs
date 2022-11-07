#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include <git2.h>

namespace fs = std::filesystem;

struct Options {
  bool debug;
  fs::path targetPath;
  Options() : debug(false), targetPath() { }
};

struct RepoDesc {
  std::string protocol;
  std::string host;
  std::string user;
  std::string repoName;
  RepoDesc(std::string protocol,
           std::string host,
           std::string user,
           std::string repoName) :
    protocol(protocol),
    host(host),
    user(user),
    repoName(repoName) { }
};

void processStoryFile(Options& options);
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
void removeFile2GitRepo(::git_repository *repo,
                        const fs::path& filePath,
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
                 Options& options);
RepoDesc* url2RepoDesc(std::string& url);
