#pragma once

#include <filesystem>

namespace ur {

// All runtime paths derived from the platform-specific root.
struct Paths {
  std::filesystem::path root;
  std::filesystem::path workspace;  // agent read/write sandbox
  std::filesystem::path database;   // SQLite database dir
  std::filesystem::path tools;      // custom tool plugins
  std::filesystem::path logs;       // runtime logs
  std::filesystem::path keys;       // API keys and credentials
};

// Resolve the platform-specific root and build all sub-paths.
// Linux:   ~/.local/share/ur/
// macOS:   ~/Library/Application Support/ur/
// Windows: %APPDATA%\ur
Paths resolve_paths();

// Create all workspace subdirectories. Safe to call multiple times
// (idempotent).
void init_workspace(const Paths& paths);

// Remove the workspace/ subdirectory under the ur root.
// Other subdirectories (database/, logs/, keys/, tools/) are left intact.
// Run ur init to recreate it.
void remove_workspace(const Paths& paths);

}  // namespace ur
