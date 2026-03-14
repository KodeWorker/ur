#pragma once

#include <filesystem>
#include <string>

#include "tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// Sandbox — tier 1 path enforcement.
//
// Tools that operate on file paths call Sandbox::validate() before any I/O.
// The sandbox does not know which JSON field contains a path — each tool is
// responsible for extracting its path argument and passing it here.
//
// Usage in a tool's execute():
//   auto err = Sandbox::validate(requested_path, policy);
//   if (!err.empty()) return {err, true};
// ---------------------------------------------------------------------------
class Sandbox {
 public:
  // Validate that requested_path is inside policy.workspace_root.
  //
  // Returns "" (empty string) if the path is allowed.
  // Returns a human-readable error message if the path is rejected.
  //
  // Immediately returns "" when policy.allow_all is true (no restriction).
  // Uses std::filesystem::weakly_canonical to resolve symlinks and ".."
  // before comparing — prevents traversal attacks.
  static std::string validate(const std::filesystem::path& requested_path,
                              const SandboxPolicy& policy);
};

}  // namespace ur
