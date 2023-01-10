#include "helper.h"
#include <sstream>
#include <string.h>

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

void moveFile2GitRepo(::git_repository *repo,
                      const fs::path& srcPath,
                      const fs::path& dstPath,
                      Options& options) {
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

struct ProgressData {
  ::git_indexer_progress fetch_progress;
  size_t completed_steps;
  size_t total_steps;
  const char *path;
};

static void
printProgress(const ProgressData *pd)
{
  int network_percent = pd->fetch_progress.total_objects > 0 ?
    (100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects :
    0;
  int index_percent = pd->fetch_progress.total_objects > 0 ?
    (100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects :
    0;

  int checkout_percent = pd->total_steps > 0
    ? (int)((100 * pd->completed_steps) / pd->total_steps)
    : 0;
  size_t kbytes = pd->fetch_progress.received_bytes / 1024;

  if (pd->fetch_progress.total_objects &&
      pd->fetch_progress.received_objects == pd->fetch_progress.total_objects) {
    std::cout << "Resolving deltas " // Format: %u/%u\r",
              <<  static_cast<unsigned int>(pd->fetch_progress.indexed_deltas)
              << '/'
              <<  static_cast<unsigned int>(pd->fetch_progress.total_deltas)
              << std::endl;
  } else {
    // printf("net %3d%% (%4" PRIuZ " kb, %5u/%5u)  /  idx %3d%% (%5u/%5u)  /  chk %3d%% (%4" PRIuZ "/%4" PRIuZ")%s\n",
    //        network_percent, kbytes,
    //        pd->fetch_progress.received_objects, pd->fetch_progress.total_objects,
    //        index_percent, pd->fetch_progress.indexed_objects, pd->fetch_progress.total_objects,
    //        checkout_percent,
    //        pd->completed_steps, pd->total_steps,
    //        pd->path);
    std::cout << "net ";
    std::cout.width(3);
    std::cout << network_percent
              << " % ("
              << kbytes
              << " "
              << pd->fetch_progress.received_objects
              << " "
              << pd->fetch_progress.total_objects
              << " "
              << index_percent
              << " kb, (";
    std::cout.width(5);
    std::cout << static_cast<unsigned int>(pd->fetch_progress.indexed_objects)
              << "/";
    std::cout.width(5);
    std::cout << static_cast<unsigned int>(pd->fetch_progress.total_objects)
              << ")  /  chk ";
    std::cout.width(3);
    std::cout << checkout_percent
              << " "
              << pd->completed_steps
              << " "
              << pd->total_steps
              << " "
              << pd->path
              << std::endl;
  }
}

static int
sidebandProgress(const char *str, int len, void *payload)
{
  (void)payload; /* unused */

  std::cout << "remote: " << len << " " << str << std::endl;;
  std::cout.flush();
  return 0;
}

static int fetchProgress(const ::git_indexer_progress *stats,
                         void *payload)
{
  ProgressData *pd = static_cast<ProgressData*>(payload);
  pd->fetch_progress = *stats;
  printProgress(pd);
  return 0;
}

static void
checkoutProgress(const char* path,
                 size_t cur,
                 size_t tot,
                 void* payload) {
  ProgressData *pd = static_cast<ProgressData*>(payload);
  pd->completed_steps = cur;
  pd->total_steps = tot;
  pd->path = path;
  printProgress(pd);
}

static int
credAcquireCb(::git_credential **out,
              const char *url,
              const char *userNameURL,
              unsigned int allowed_types,
              void *payload) {
  std::string userName { userNameURL } ;
  std::string passWord { "" };
  unsigned int credentialTypes[] = { GIT_CREDENTIAL_SSH_KEY,
                                     GIT_CREDENTIAL_USERPASS_PLAINTEXT,
                                     GIT_CREDENTIAL_USERNAME };
  static int nextTypeToTry = -1;

  int status = GIT_ERROR;

  nextTypeToTry++;
  std::cout << "Trying to get a credential"
            << " nextTypetoTry: " << nextTypeToTry
            << " sizeof(credentialTypes) "
            << (sizeof(credentialTypes) / sizeof(unsigned int))
            << " currentType: " << credentialTypes[nextTypeToTry]
            << std::endl;
  if (nextTypeToTry < sizeof(credentialTypes) / sizeof(int)) {
    switch (credentialTypes[nextTypeToTry]) {
    case GIT_CREDENTIAL_SSH_KEY:
      if (allowed_types & credentialTypes[nextTypeToTry]) {
        fs::path privKey { "/home/juancardona/.ssh/id_rsa" };
        fs::path pubKey  { privKey };
        pubKey.replace_extension(".pub");

        std::cout << "Url: " << url << std::endl
                  << "Username: " << userName << std::endl
                  << "Private Key: " << privKey << std::endl
                  << "Public Key: " << pubKey << std::endl
                  << "Password: " << passWord << std::endl;
        status = ::git_credential_ssh_key_new(out,
                                              userName.c_str(),
                                              pubKey.c_str(),
                                              privKey.c_str(),
                                              passWord.c_str());
      }
      break;
    case GIT_CREDENTIAL_USERPASS_PLAINTEXT:
      if (allowed_types & credentialTypes[nextTypeToTry]) {
        std::cout << "Password: ";
        std::cout.flush();
        std::cin >> passWord;
        status = ::git_credential_userpass_plaintext_new(out,
                                                         userName.c_str(),
                                                         passWord.c_str());
      }
      break;
    case GIT_CREDENTIAL_USERNAME:
      if (allowed_types & GIT_CREDENTIAL_USERNAME) {
        status = ::git_credential_username_new(out,
                                               userName.c_str());
      }
      break;
    default:
      status = GIT_ERROR;
      break;
    }
  }
  else {
    status = GIT_ERROR;
  }

  std::cout << "Status: " << status << std::endl;

  return status;
}

int
cloneGitRepo(fs::path& location,
             std::string& url,
	     RepoDesc* rd,
             Options& options) {
  ProgressData pd = { {0} };
  // ::git_repository *clonedRepo = nullptr;
  ::git_clone_options cloneOpts = GIT_CLONE_OPTIONS_INIT;
  ::git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  int error;

  checkoutOpts.checkout_strategy = GIT_CHECKOUT_SAFE;
  checkoutOpts.progress_cb = checkoutProgress;
  checkoutOpts.progress_payload = &pd;
  cloneOpts.checkout_opts = checkoutOpts;
  cloneOpts.fetch_opts.callbacks.sideband_progress = sidebandProgress;
  cloneOpts.fetch_opts.callbacks.transfer_progress = &fetchProgress;
  cloneOpts.fetch_opts.callbacks.credentials = credAcquireCb;
  cloneOpts.fetch_opts.callbacks.payload = &pd;

  std::cout << "Cloning: " << url << " at " << location << std::endl;
  error = git_clone(&rd->repo, url.c_str(), location.c_str(), nullptr); // &cloneOpts);
  std::cout << std::endl;

  if (error != 0) {
    const git_error *err = ::git_error_last();
    if (err) {
      std::cerr << "ERROR "
                << err->klass
                << ":"
                << err->message
                << std::endl;
    }
    else {
      std::cerr << "ERROR "
                << error
                << " no detailed info"
                << std::endl;
    }
  }
  // else if (clonedRepo) {
  //   // ::git_repository_free(clonedRepo);
    
  // }
  return error;
}

RepoDesc*
url2RepoDesc(std::string& url) {
  RepoDesc *retValue = nullptr;

  const std::regex line_regex("(https)://(.*)/(.*)/(.*)\\.git");
  std::smatch repoInfo;

  if (std::regex_match(url,repoInfo,line_regex)) {
    retValue = new RepoDesc(repoInfo[1],
                            repoInfo[2],
                            repoInfo[3],
                            repoInfo[4]);
  }

  return retValue;
}

static int
getAnnotatedCommitFromName(::git_annotated_commit** commit,
			   ::git_repository* repo,
			   const std::string& name) {
  ::git_reference *ref;
  int error = ::git_reference_dwim(&ref, repo, name.c_str());
  
  if (error == GIT_OK) {
    ::git_annotated_commit_from_ref(commit, repo, ref);
    ::git_reference_free(ref);
  }
  else {
    ::git_reference_free(ref);
    ::git_object *obj;
    
    error = ::git_revparse_single(&obj, repo, name.c_str());
    
    if (error == GIT_OK) {
      ::git_annotated_commit_lookup(commit, repo, ::git_object_id(obj));
      ::git_object_free(obj);
    }
  }

  return error;
}

static int
getAnnotatedCommitFromGuessingName(::git_annotated_commit** commit,
				   ::git_repository* repo,
				   const std::string& name) {
  ::git_strarray   remotes { nullptr, 0 };
  ::git_reference* remote_ref  { nullptr };
  int error;

  if ((error = ::git_remote_list(&remotes, repo)) < GIT_OK) {
    ::git_reference_free(remote_ref);
    ::git_strarray_dispose(&remotes);

    return error;
  }
 
  size_t i;
  for (i = 0; i < remotes.count; i++) {
    std::stringstream refname;

    refname << "ref/remotes/" << remotes.strings[i] << "/" << name;
    
    if ((error = ::git_reference_lookup(&remote_ref, repo, refname.str().c_str())) < GIT_OK)
      if (error < 0 and error != GIT_ENOTFOUND) break;

    break;
  }

  if (!remote_ref) {
    error = GIT_ENOTFOUND;
    ::git_reference_free(remote_ref);
    ::git_strarray_dispose(&remotes);
    return error;
  }

  error = ::git_annotated_commit_from_ref(commit, repo, remote_ref);
  
  ::git_reference_free(remote_ref);
  ::git_strarray_dispose(&remotes);
  return error; 
}

static int
performCheckoutRef(::git_repository *repo,
		   ::git_annotated_commit *target,
		   const std::string& target_ref) {
  ::git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
  ::git_reference *ref = NULL, *branch = NULL;
  ::git_commit *target_commit = NULL;
  int error;

  /** Setup our checkout options from the parsed options */
  checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  // if (opts->force)
  //   checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;

  // if (opts->progress)
  //   checkout_opts.progress_cb = print_checkout_progress;

  // if (opts->perf)
  //   checkout_opts.perfdata_cb = print_perf_data;

  /** Grab the commit we're interested to move to */
  error = ::git_commit_lookup(&target_commit, repo, git_annotated_commit_id(target));
  
  if (error != GIT_OK) {
    std::cerr << "Failed to lookup commit: "
	      << git_error_last()->message << std::endl;
    ::git_commit_free(target_commit);
    ::git_reference_free(branch);
    ::git_reference_free(ref);

    return error;
  }

  /**
   * Perform the checkout so the workdir corresponds to what target_commit
   * contains.
   *
   * Note that it's okay to pass a git_commit here, because it will be
   * peeled to a tree.
   */
  error = ::git_checkout_tree(repo, (const git_object *)target_commit, &checkout_opts);
  
  if (error != GIT_OK) {
    std::cerr << "failed to checkout tree: "
	      << git_error_last()->message
	      << std::endl;
    
    ::git_commit_free(target_commit);
    ::git_reference_free(branch);
    ::git_reference_free(ref);

    return error;
  }

  /**
   * Now that the checkout has completed, we have to update HEAD.
   *
   * Depending on the "origin" of target (ie. it's an OID or a branch name),
   * we might need to detach HEAD.
   */
  if (::git_annotated_commit_ref(target)) {
    const char *target_head;

    if ((error = ::git_reference_lookup(&ref, repo, git_annotated_commit_ref(target))) < 0) {
      if (error != 0) {
	std::cerr << "failed to update HEAD reference: "
		  << git_error_last()->message << std::endl;
	::git_commit_free(target_commit);
	::git_reference_free(branch);
	::git_reference_free(ref);
	
	return error;
      }
    }
    
    if (::git_reference_is_remote(ref)) {
      if ((error = ::git_branch_create_from_annotated(&branch, repo, target_ref.c_str(), target, 0)) < 0) {
	std::cerr << "failed to update HEAD reference: "
		  << ::git_error_last()->message << std::endl;
	::git_commit_free(target_commit);
	::git_reference_free(branch);
	::git_reference_free(ref);
	
	return error;
      }
      target_head = ::git_reference_name(branch);
    } else {
      target_head = ::git_annotated_commit_ref(target);
    }

    error = ::git_repository_set_head(repo, target_head);
  } else {
    error = ::git_repository_set_head_detached_from_annotated(repo, target);
  }

  if (error != 0) {
    std::cerr << "failed to update HEAD reference: "
	      << ::git_error_last()->message << std::endl;
  }
  
  ::git_commit_free(target_commit);
  ::git_reference_free(branch);
  ::git_reference_free(ref);

  return error;
}


int
checkoutGitRepoFromName(::git_repository* repo,
			const std::string& name,
			Options& options) {
  ::git_annotated_commit *commit;
  int error;

  if ((error = getAnnotatedCommitFromName(&commit, repo, name) < GIT_OK) and
      (error = getAnnotatedCommitFromGuessingName(&commit, repo, name) < GIT_OK)) {
    std::cerr << "Failed to resolve " << name << ": "
	      << ::git_error_last()->message << std::endl;
    return error;
  }

  error = performCheckoutRef(repo,
			     commit,
			     name);

  ::git_annotated_commit_free(commit);
  return error;
}
