#include "env.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace ur {

namespace {

std::string strip_quotes(const std::string& s) {
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                        (s.front() == '\'' && s.back() == '\''))) {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

}  // namespace

void load_dotenv(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) return;

  std::string line;
  while (std::getline(file, line)) {
    // Skip blank lines and comments.
    if (line.empty() || line[0] == '#') continue;

    auto eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = line.substr(0, eq);
    std::string val = strip_quotes(line.substr(eq + 1));

    if (key.empty()) continue;

// Intentional: .env is project-level defaults that always win. Shell-profile
// values take precedence by simply not listing the var in .env. Skipping
// already-set vars would make .env silently ineffective when the same var
// appears in both places.
#ifdef _WIN32
    _putenv_s(key.c_str(), val.c_str());
#else
    setenv(key.c_str(), val.c_str(), /*overwrite=*/1);
#endif
  }
}

void save_dotenv(const std::filesystem::path& path,
                 const std::map<std::string, std::string>& vars) {
  // Read existing lines, replacing matching keys in-place.
  std::vector<std::string> lines;
  std::map<std::string, bool> written;
  for (const auto& kv : vars) written[kv.first] = false;

  std::ifstream in(path);
  if (in) {
    std::string line;
    while (std::getline(in, line)) {
      auto eq = line.find('=');
      if (eq != std::string::npos) {
        std::string key = line.substr(0, eq);
        auto it = vars.find(key);
        if (it != vars.end()) {
          lines.push_back(key + "=" + it->second);
          written[key] = true;
          continue;
        }
      }
      lines.push_back(line);
    }
    in.close();
  }

  // Append keys not already present.
  for (const auto& kv : vars) {
    if (!written[kv.first]) lines.push_back(kv.first + "=" + kv.second);
  }

  std::ofstream out(path);
  for (const auto& l : lines) out << l << "\n";

  if (!out.is_open() || out.fail()) {
    throw std::runtime_error("failed to write .env file: " + path.string());
  }
  std::filesystem::permissions(
      path,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace);
}

int env_int(const char* name, int fallback) {
  const char* v = std::getenv(name);
  if (!v || !v[0]) return fallback;
  try {
    return std::stoi(v);
  } catch (...) {
    std::cerr << "warning: invalid integer in env var " << name
              << ", using fallback " << fallback << "\n";
    return fallback;
  }
}

}  // namespace ur
