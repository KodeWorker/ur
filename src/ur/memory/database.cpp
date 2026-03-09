#include "database.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "crypto.hpp"

namespace ur {

// Encrypt content if key_ is set; return as-is otherwise.
// Use before every write of a message content or persona value.
// Decrypt content if key_ is set; return as-is otherwise.
// Use after every read of a message content or persona value.

Database::Database(std::filesystem::path path, std::string key)
    : path_(std::move(path)), key_(std::move(key)) {}

Database::~Database() = default;

bool Database::is_open() const { return handle_ != nullptr; }

std::string Database::enc(const std::string& str) const {
  return key_.empty() ? str : encrypt(str, key_);
}

std::string Database::dec(const std::string& str) const {
  return key_.empty() ? str : decrypt(str, key_);
}

void Database::open() {
  sqlite3* db_handle = nullptr;
  int rc = sqlite3_open(path_.string().c_str(), &db_handle);
  handle_.reset(db_handle);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    handle_.reset();
    throw std::runtime_error("Database::open: " + err_msg);
  }
}

void Database::init_schema() {
  // Note: when binding message.content or persona.value on INSERT/UPDATE,
  //       wrap the value with enc(). When reading those columns on SELECT,
  //       wrap with dec(). Schema creation itself needs no encryption.
  if (!is_open()) open();
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS session (
        id         TEXT PRIMARY KEY,
        title      TEXT,
        model      TEXT,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS message (
        id         TEXT PRIMARY KEY,
        session_id TEXT NOT NULL REFERENCES session(id),
        role       TEXT NOT NULL,
        content    TEXT NOT NULL,
        created_at INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS persona (
        key        TEXT PRIMARY KEY,
        value      TEXT NOT NULL,
        updated_at INTEGER NOT NULL
    );
  )";
  char* err_msg = nullptr;
  int rc = sqlite3_exec(handle_.get(), sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string err_str = err_msg ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Database::init_schema: " + err_str);
  }
}

void Database::drop_all() {
  // Drop message before session to respect the foreign key reference.
  if (!is_open()) open();
  const char* sql = R"(
    DROP TABLE IF EXISTS message;
    DROP TABLE IF EXISTS session;
    DROP TABLE IF EXISTS persona;
  )";
  char* err_msg = nullptr;
  int rc = sqlite3_exec(handle_.get(), sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string err_str = err_msg ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Database::drop_all: " + err_str);
  }
}

}  // namespace ur
