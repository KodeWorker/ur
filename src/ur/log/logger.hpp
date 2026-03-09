#pragma once

#include <filesystem>
#include <fstream>
#include <string_view>

namespace ur {

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

// Parses UR_LOG_LEVEL env var (case-insensitive). Returns INFO if
// unset/invalid.
LogLevel log_level_from_env();

class Logger {
 public:
  // log_dir: directory where ur.log is written (lazy — file not opened until
  // first write). min_level: messages below this level are discarded.
  explicit Logger(std::filesystem::path log_dir,
                  LogLevel min_level = LogLevel::INFO);

  void debug(std::string_view msg);
  void info(std::string_view msg);
  void warn(std::string_view msg);
  void error(std::string_view msg);

 private:
  void write(LogLevel level, std::string_view msg);
  std::ofstream& stream();

  std::filesystem::path path_;
  LogLevel min_level_;
  std::ofstream file_;
  bool open_failed_ = false;
};

}  // namespace ur
