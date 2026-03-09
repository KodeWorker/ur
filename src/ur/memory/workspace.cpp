#include "workspace.hpp"

#include <stdexcept>
#include <string>

namespace ur {

Paths resolve_paths() {
  // detect platform and resolve the root directory:
  //   Linux:   $XDG_DATA_HOME/ur  (fallback: ~/.local/share/ur)
  //   macOS:   ~/Library/Application Support/ur
  //   Windows: %APPDATA%\ur
  // Then build and return a Paths struct with all five subdirs.
  std::filesystem::path root;
#if defined(__linux__)
  const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
  if (xdg_data_home) {
    root = xdg_data_home;
  } else {
    const char* home = std::getenv("HOME");
    if (!home) {
      throw std::runtime_error(
          "resolve_paths: HOME environment variable not set");
    }
    root = std::filesystem::path(home) / ".local" / "share";
  }
  root /= "ur";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home) {
    throw std::runtime_error(
        "resolve_paths: HOME environment variable not set");
  }
  root = std::filesystem::path(home) / "Library" / "Application Support" / "ur";
#elif defined(_WIN32)
  const char* appdata = std::getenv("APPDATA");
  if (!appdata) {
    throw std::runtime_error(
        "resolve_paths: APPDATA environment variable not set");
  }
  root = std::filesystem::path(appdata) / "ur";
#else
  throw std::runtime_error("resolve_paths: unsupported platform");
#endif

  Paths paths;
  paths.root = root;
  paths.workspace = root / "workspace";
  paths.database = root / "database";
  paths.tools = root / "tools";
  paths.logs = root / "logs";
  paths.keys = root / "keys";
  return paths;
}

void init_workspace(const Paths& paths) {
  //  create paths.workspace, paths.database, paths.tools,
  //  paths.logs, and paths.keys using std::filesystem::create_directories.
  //  create_directories is idempotent — no need to check existence first.
  try {
    std::filesystem::create_directories(paths.workspace);
    std::filesystem::create_directories(paths.database);
    std::filesystem::create_directories(paths.tools);
    std::filesystem::create_directories(paths.logs);
    std::filesystem::create_directories(paths.keys);
  } catch (const std::filesystem::filesystem_error& e) {
    throw std::runtime_error(
        std::string("init_workspace: failed to create directories: ") +
        e.what());
  }
}

void remove_workspace(const Paths& paths) {
  for (auto& entry : std::filesystem::directory_iterator(paths.workspace)) {
    std::filesystem::remove_all(entry.path());
  }
}

}  // namespace ur
