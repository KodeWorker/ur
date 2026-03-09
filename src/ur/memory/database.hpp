#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <string>

// Forward-declare sqlite3 to avoid exposing it in the public header.
struct sqlite3;

namespace ur {

class Database {
 public:
  // Stores the path and optional encryption key — does NOT open the file.
  // key: raw bytes of the AES-256-GCM key loaded from keys/secret.key.
  //      Empty string disables encryption (plaintext mode).
  explicit Database(std::filesystem::path path, std::string key = {});

  ~Database();

  // Non-copyable, movable.
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) = default;
  Database& operator=(Database&&) = default;

  // Lazily opens the database file, then creates all three tables if they
  // do not already exist. Safe to call multiple times (idempotent).
  void init_schema();

  // Lazily opens the database file, then drops all tables.
  // The database file itself is kept; only the schema is removed.
  void drop_all();

  bool is_open() const;

 private:
  // Opens the file at path_ if not already open.
  // Throws std::runtime_error on failure.
  void open();

  // Encrypt str if key_ is set; return str unchanged otherwise.
  std::string enc(const std::string& str) const;
  // Decrypt str if key_ is set; return str unchanged otherwise.
  std::string dec(const std::string& str) const;

  std::filesystem::path path_;
  // cppcheck-suppress unusedStructMember
  std::string key_;  // empty = plaintext mode
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> handle_{nullptr,
                                                             sqlite3_close};
};

}  // namespace ur
