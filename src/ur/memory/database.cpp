#include "database.hpp"

#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace ur {

Database::Database(std::filesystem::path path) : path_(std::move(path)) {}

Database::~Database() {
  // TODO: close handle_ with sqlite3_close() if it is not nullptr.
}

bool Database::is_open() const { return handle_ != nullptr; }

void Database::open() {
  // TODO: call sqlite3_open() on path_.string().
  //       On failure throw std::runtime_error with the sqlite3 error message.
  throw std::runtime_error("Database::open: not implemented");
}

void Database::init_schema() {
  if (!is_open())
    open();

  // TODO: execute the following SQL via sqlite3_exec():
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
  if (!is_open())
    open();

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

} // namespace ur
