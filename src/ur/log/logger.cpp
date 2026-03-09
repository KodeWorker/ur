#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>

namespace ur {

namespace {

const char* level_str(LogLevel l) {
  switch (l) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO ";
    case LogLevel::WARN:
      return "WARN ";
    case LogLevel::ERROR:
      return "ERROR";
  }
  __builtin_unreachable();
}

std::string timestamp() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[20];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  return buf;
}

}  // namespace

LogLevel log_level_from_env() {
  const char* val = std::getenv("UR_LOG_LEVEL");
  if (!val) return LogLevel::INFO;

  std::string s(val);
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (s == "debug") return LogLevel::DEBUG;
  if (s == "info") return LogLevel::INFO;
  if (s == "warn") return LogLevel::WARN;
  if (s == "error") return LogLevel::ERROR;

  return LogLevel::INFO;
}

Logger::Logger(std::filesystem::path log_dir, LogLevel min_level)
    : path_(std::move(log_dir) / "ur.log"), min_level_(min_level) {}

std::ofstream& Logger::stream() {
  if (!file_.is_open() && !open_failed_) {
    file_.open(path_, std::ios::app);
    if (!file_.is_open()) open_failed_ = true;
  }
  return file_;
}

void Logger::write(LogLevel level, std::string_view msg) {
  if (level < min_level_) return;

  // File: full line with timestamp.
  auto& f = stream();
  if (f.is_open()) {
    f << '[' << timestamp() << "] [" << level_str(level) << "] " << msg << '\n';
    f.flush();
  }

  // Stderr: short label only for WARN and ERROR.
  if (level >= LogLevel::WARN) {
    std::cerr << '[' << level_str(level) << "] " << msg << '\n';
  }
}

void Logger::debug(std::string_view msg) { write(LogLevel::DEBUG, msg); }
void Logger::info(std::string_view msg) { write(LogLevel::INFO, msg); }
void Logger::warn(std::string_view msg) { write(LogLevel::WARN, msg); }
void Logger::error(std::string_view msg) { write(LogLevel::ERROR, msg); }

}  // namespace ur
