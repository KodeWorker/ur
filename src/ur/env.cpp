#include "env.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

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

#ifdef _WIN32
    _putenv_s(key.c_str(), val.c_str());
#else
    setenv(key.c_str(), val.c_str(), /*overwrite=*/1);
#endif
  }
}

}  // namespace ur
