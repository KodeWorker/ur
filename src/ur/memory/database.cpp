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

Database::~Database() {
  // TODO: close handle_ with sqlite3_close() if it is not nullptr.
}

bool Database::is_open() const { return handle_ != nullptr; }

std::string Database::enc(const std::string& str) const {
  return key_.empty() ? str : encrypt(str, key_);
}

std::string Database::dec(const std::string& str) const {
  return key_.empty() ? str : decrypt(str, key_);
}

void Database::open() {
  // TODO: call sqlite3_open() on path_.string().
  //       On failure throw std::runtime_error with the sqlite3 error message.
  throw std::runtime_error("Database::open: not implemented");
}

void Database::init_schema() {
  if (!is_open()) open();

  // TODO: execute the following SQL via sqlite3_exec():
  // Note: when binding message.content or persona.value on INSERT/UPDATE,
  //       wrap the value with enc(). When reading those columns on SELECT,
  //       wrap with dec(). Schema creation itself needs no encryption.
  //
  // CREATE TABLE IF NOT EXISTS session (
  //     id         TEXT PRIMARY KEY,
  //     title      TEXT,
  //     model      TEXT,
  //     created_at INTEGER NOT NULL,
  //     updated_at INTEGER NOT NULL
  // );
  //
  // CREATE TABLE IF NOT EXISTS message (
  //     id         TEXT PRIMARY KEY,
  //     session_id TEXT NOT NULL REFERENCES session(id),
  //     role       TEXT NOT NULL,
  //     content    TEXT NOT NULL,
  //     created_at INTEGER NOT NULL
  // );
  //
  // CREATE TABLE IF NOT EXISTS persona (
  //     key        TEXT PRIMARY KEY,
  //     value      TEXT NOT NULL,
  //     updated_at INTEGER NOT NULL
  // );
  //
  // On failure throw std::runtime_error with the sqlite3 error message.
  throw std::runtime_error("Database::init_schema: not implemented");
}

void Database::drop_all() {
  if (!is_open()) open();

  // TODO: execute the following SQL via sqlite3_exec():
  //
  // DROP TABLE IF EXISTS message;
  // DROP TABLE IF EXISTS session;
  // DROP TABLE IF EXISTS persona;
  //
  // Drop message before session to respect the foreign key reference.
  // On failure throw std::runtime_error with the sqlite3 error message.
  throw std::runtime_error("Database::drop_all: not implemented");
}

}  // namespace ur
