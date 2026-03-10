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

void Database::insert_session(const std::string& id, const std::string& title,
                              const std::string& model, int64_t created_at,
                              int64_t updated_at) {
  if (!is_open()) open();
  // SQL: parameterized INSERT into session (id, title, model, created_at,
  // updated_at).
  const char* sql = R"(
    INSERT INTO session (id, title, model, created_at, updated_at)
    VALUES (?, ?, ?, ?, ?)
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::insert_session (prepare): " + err_msg);
  }
  // Bind parameters
  if (sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_session (bind id): " + err_msg);
  }
  if (sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_session (bind title): " +
                             err_msg);
  }
  if (sqlite3_bind_text(stmt, 3, model.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_session (bind model): " +
                             err_msg);
  }
  if (sqlite3_bind_int64(stmt, 4, created_at) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_session (bind created_at): " +
                             err_msg);
  }
  if (sqlite3_bind_int64(stmt, 5, updated_at) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_session (bind updated_at): " +
                             err_msg);
  }
  // Execute
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_session (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
}

void Database::insert_message(const std::string& id,
                              const std::string& session_id,
                              const std::string& role,
                              const std::string& content, int64_t created_at) {
  if (!is_open()) open();
  // SQL: parameterized INSERT into message (id, session_id, role, content,
  // created_at).
  const char* sql = R"(
    INSERT INTO message (id, session_id, role, content, created_at)
    VALUES (?, ?, ?, ?, ?)
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::insert_message (prepare): " + err_msg);
  }
  // Bind parameters
  if (sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_message (bind id): " + err_msg);
  }
  if (sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_message (bind session_id): " +
                             err_msg);
  }
  if (sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_message (bind role): " +
                             err_msg);
  }
  const std::string encrypted = enc(content);
  if (sqlite3_bind_blob(stmt, 4, encrypted.data(),
                        static_cast<int>(encrypted.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_message (bind content): " +
                             err_msg);
  }
  if (sqlite3_bind_int64(stmt, 5, created_at) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_message (bind created_at): " +
                             err_msg);
  }
  // Execute
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::insert_message (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
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
