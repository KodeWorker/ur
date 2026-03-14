#include "sandbox.hpp"

#include <filesystem>
#include <string>

namespace ur {

// static
std::string Sandbox::validate(const std::filesystem::path& requested_path,
                              const SandboxPolicy& policy) {
  if (policy.allow_all) return "";

  // TODO: implement tier 1 path enforcement
  //
  // Steps:
  //   1. std::filesystem::weakly_canonical(requested_path) to resolve symlinks
  //      and ".." without requiring the path to exist.
  //   2. Confirm the resolved path equals workspace_root OR starts with
  //      workspace_root + "/".  A simple string prefix check is NOT sufficient
  //      (e.g. "/workspace_rootExtra" would pass) — compare path components.
  //   3. On violation return:
  //        "path outside workspace: " + resolved.string()
  //   4. On success return "".
  //
  // Note: weakly_canonical follows symlinks for all intermediate components
  // but not the final component if it doesn't exist.  A symlink inside the
  // workspace pointing outside is still caught because the target is returned
  // as the canonical path, which then fails the prefix check.

  (void)requested_path;
  (void)policy;
  return "";  // replace with real implementation
}

}  // namespace ur
