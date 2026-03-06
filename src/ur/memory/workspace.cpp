#include "workspace.hpp"

#include <stdexcept>

namespace ur {

Paths resolve_paths() {
  // TODO: detect platform and resolve the root directory:
  //   Linux:   $XDG_DATA_HOME/ur  (fallback: ~/.local/share/ur)
  //   macOS:   ~/Library/Application Support/ur
  //   Windows: %APPDATA%\ur
  // Then build and return a Paths struct with all five subdirs.
  throw std::runtime_error("resolve_paths: not implemented");
}

void init_workspace(const Paths &paths) {
  // TODO: create paths.workspace, paths.database, paths.tools,
  //       paths.log, and paths.keys using std::filesystem::create_directories.
  //       create_directories is idempotent — no need to check existence first.
  (void)paths;
  throw std::runtime_error("init_workspace: not implemented");
}

void remove_workspace(const Paths &paths) {
  // TODO: remove the contents of paths.workspace and paths.database
  //       using std::filesystem::remove_all.
  (void)paths;
  throw std::runtime_error("remove_workspace: not implemented");
}

} // namespace ur
