#include "database.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "crypto.hpp"

namespace ur {

Database::Database(std::filesystem::path path, std::string key)
    : path_(std::move(path)), key_(std::move(key)) {}

Database::~Database() = default;

bool Database::is_open() const { return handle_ != nullptr; }

std::string Database::enc(const std::string& str) const {
  return encrypt(str, key_);
}

std::string Database::dec(const std::string& str) const {
  return decrypt(str, key_);
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

    CREATE UNIQUE INDEX IF NOT EXISTS idx_session_title
        ON session(title) WHERE title IS NOT NULL AND title != '';
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
  {
    const int title_rc =
        title.empty()
            ? sqlite3_bind_null(stmt, 2)
            : sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    if (title_rc != SQLITE_OK) {
      std::string err_msg = sqlite3_errmsg(handle_.get());
      sqlite3_finalize(stmt);
      throw std::runtime_error("Database::insert_session (bind title): " +
                               err_msg);
    }
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

void Database::begin() {
  if (!is_open()) open();
  char* err_msg = nullptr;
  if (sqlite3_exec(handle_.get(), "BEGIN", nullptr, nullptr, &err_msg) !=
      SQLITE_OK) {
    std::string err = err_msg ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Database::begin: " + err);
  }
}

void Database::commit() {
  char* err_msg = nullptr;
  if (sqlite3_exec(handle_.get(), "COMMIT", nullptr, nullptr, &err_msg) !=
      SQLITE_OK) {
    std::string err = err_msg ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Database::commit: " + err);
  }
}

void Database::rollback() {
  char* err_msg = nullptr;
  if (sqlite3_exec(handle_.get(), "ROLLBACK", nullptr, nullptr, &err_msg) !=
      SQLITE_OK) {
    std::string err = err_msg ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Database::rollback: " + err);
  }
}

bool Database::session_exists(const std::string& id) {
  if (!is_open()) open();
  const char* sql = R"(
    SELECT 1 FROM session WHERE id=? LIMIT 1
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::session_exists (prepare): " + err_msg);
  }
  if (sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::session_exists (bind id): " + err_msg);
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::session_exists (step): " + err_msg);
  }
  const bool exists = (rc == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return exists;
}

static std::string col_text(sqlite3_stmt* stmt, int col) {
  const auto* p = sqlite3_column_text(stmt, col);
  return p ? reinterpret_cast<const char*>(p) : std::string{};
}

static std::string col_blob(sqlite3_stmt* stmt, int col) {
  const void* p = sqlite3_column_blob(stmt, col);
  int n = sqlite3_column_bytes(stmt, col);
  return p ? std::string(static_cast<const char*>(p), n) : std::string{};
}

std::vector<SessionRow> Database::select_sessions() {
  if (!is_open()) open();
  const char* sql = R"(
    SELECT id, title, model, created_at, updated_at
    FROM session
    ORDER BY created_at DESC
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::select_sessions (prepare): " + err_msg);
  }
  std::vector<SessionRow> sessions;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    sessions.push_back(SessionRow{
        col_text(stmt, 0), col_text(stmt, 1), col_text(stmt, 2),
        sqlite3_column_int64(stmt, 3), sqlite3_column_int64(stmt, 4)});
  }
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::select_sessions (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
  return sessions;
}

std::vector<MessageRow> Database::select_messages(
    const std::string& session_id) {
  if (!is_open()) open();
  const char* sql = R"(
    SELECT id, session_id, role, content, created_at
    FROM message
    WHERE session_id=?
    ORDER BY created_at ASC
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::select_messages (prepare): " + err_msg);
  }
  if (sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::select_messages (bind session_id): " +
                             err_msg);
  }
  std::vector<MessageRow> messages;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    messages.push_back(MessageRow{col_text(stmt, 0), col_text(stmt, 1),
                                  col_text(stmt, 2), dec(col_blob(stmt, 3)),
                                  sqlite3_column_int64(stmt, 4)});
  }
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::select_messages (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
  return messages;
}

std::vector<PersonaRow> Database::select_persona(size_t limit) {
  if (!is_open()) open();
  const std::string sql =
      "SELECT key, value, updated_at FROM persona ORDER BY updated_at DESC" +
      (limit > 0 ? " LIMIT " + std::to_string(limit) : "");
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::select_persona (prepare): " + err_msg);
  }
  std::vector<PersonaRow> persona;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    persona.push_back(PersonaRow{col_text(stmt, 0), dec(col_blob(stmt, 1)),
                                 sqlite3_column_int64(stmt, 2)});
  }
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::select_persona (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
  return persona;
}

void Database::upsert_persona(const std::string& key, const std::string& value,
                              int64_t updated_at) {
  if (!is_open()) open();
  const char* sql = R"(
    INSERT INTO persona (key, value, updated_at) VALUES (?, ?, ?)
    ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::upsert_persona (prepare): " + err_msg);
  }
  if (sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::upsert_persona (bind key): " + err_msg);
  }
  const std::string encrypted_value = enc(value);
  if (sqlite3_bind_blob(stmt, 2, encrypted_value.data(),
                        static_cast<int>(encrypted_value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::upsert_persona (bind value): " +
                             err_msg);
  }
  if (sqlite3_bind_int64(stmt, 3, updated_at) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::upsert_persona (bind updated_at): " +
                             err_msg);
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::upsert_persona (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
}

void Database::touch_session(const std::string& id, int64_t updated_at) {
  if (!is_open()) open();
  const char* sql = R"(
    UPDATE session SET updated_at=? WHERE id=?
  )";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    throw std::runtime_error("Database::touch_session (prepare): " + err_msg);
  }
  if (sqlite3_bind_int64(stmt, 1, updated_at) != SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::touch_session (bind updated_at): " +
                             err_msg);
  }
  if (sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::touch_session (bind id): " + err_msg);
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    std::string err_msg = sqlite3_errmsg(handle_.get());
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::touch_session (step): " + err_msg);
  }
  sqlite3_finalize(stmt);
}

std::string Database::find_session_by_id_prefix(const std::string& prefix) {
  if (!is_open()) open();
  const char* sql =
      "SELECT id FROM session WHERE id LIKE ? || '%' ORDER BY created_at DESC";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Database::find_session_by_id_prefix (prepare): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  if (sqlite3_bind_text(stmt, 1, prefix.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::find_session_by_id_prefix (bind): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  std::string found_id;
  int count = 0;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (count == 0) found_id = col_text(stmt, 0);
    ++count;
  }
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    throw std::runtime_error("Database::find_session_by_id_prefix (step): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  if (count == 0)
    throw std::runtime_error("no session with id prefix: " + prefix);
  if (count > 1)
    throw std::runtime_error("ambiguous id prefix (matches " +
                             std::to_string(count) + " sessions): " + prefix);
  return found_id;
}

void Database::update_session_title(const std::string& id,
                                    const std::string& title) {
  if (!is_open()) open();
  const char* sql = "UPDATE session SET title=? WHERE id=?";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Database::update_session_title (prepare): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  const int title_rc = title.empty() ? sqlite3_bind_null(stmt, 1)
                                     : sqlite3_bind_text(stmt, 1, title.c_str(),
                                                         -1, SQLITE_TRANSIENT);
  if (title_rc != SQLITE_OK) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::update_session_title (bind title): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  if (sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::update_session_title (bind id): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    throw std::runtime_error("Database::update_session_title (step): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
}

std::string Database::find_session_by_title(const std::string& title) {
  if (!is_open()) open();
  const char* sql = "SELECT id FROM session WHERE title=? LIMIT 1";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(handle_.get(), sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Database::find_session_by_title (prepare): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  if (sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::find_session_by_title (bind title): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Database::find_session_by_title (step): " +
                             std::string(sqlite3_errmsg(handle_.get())));
  }
  if (rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("no session with title: " + title);
  }
  std::string found_id = col_text(stmt, 0);
  sqlite3_finalize(stmt);
  return found_id;
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

void Database::drop_persona() {
  if (!is_open()) open();
  const char* sql = "DROP TABLE IF EXISTS persona;";
  char* err_msg = nullptr;
  int rc = sqlite3_exec(handle_.get(), sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string err_str = err_msg ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Database::drop_persona: " + err_str);
  }
}

}  // namespace ur
