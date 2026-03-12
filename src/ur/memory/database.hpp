#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ur {

// ---------------------------------------------------------------------------
// Row types returned by select queries (content fields are decrypted)
// ---------------------------------------------------------------------------

struct SessionRow {
  std::string id;
  std::string title;
  std::string model;
  int64_t created_at;
  int64_t updated_at;
};

struct MessageRow {
  std::string id;
  std::string session_id;
  std::string role;     // "user" | "assistant" | "reason"
  std::string content;  // decrypted
  int64_t created_at;
};

struct PersonaRow {
  std::string key;
  std::string value;  // decrypted
  int64_t updated_at;
};

class Database {
 public:
  // Stores the path and encryption key — does NOT open the file.
  // key: 32 raw bytes of the AES-256-GCM key loaded from key/secret.key.
  explicit Database(std::filesystem::path path, std::string key);

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

  // Insert a new session row. created_at and updated_at are Unix timestamps.
  // Throws std::runtime_error on failure.
  void insert_session(const std::string& id, const std::string& title,
                      const std::string& model, int64_t created_at,
                      int64_t updated_at);

  // Insert a new message row. content is stored as-is (caller is responsible
  // for encryption). Throws std::runtime_error on failure.
  void insert_message(const std::string& id, const std::string& session_id,
                      const std::string& role, const std::string& content,
                      int64_t created_at);

  // Transaction control. begin() lazily opens the database.
  // commit() and rollback() require an open handle (call after begin()).
  // All three throw std::runtime_error on failure.
  void begin();
  void commit();
  void rollback();

  // Returns true if a session with the given id exists.
  bool session_exists(const std::string& id);

  // Returns all sessions ordered by created_at DESC.
  std::vector<SessionRow> select_sessions();

  // Returns all messages for a session ordered by created_at ASC.
  // content fields are decrypted automatically.
  std::vector<MessageRow> select_messages(const std::string& session_id);

  // Returns all persona key-value pairs ordered by key ASC.
  // value fields are decrypted automatically.
  std::vector<PersonaRow> select_persona();

  // Insert or update a persona entry. Uses SQLite UPSERT (ON CONFLICT).
  void upsert_persona(const std::string& key, const std::string& value,
                      int64_t updated_at);

  // Update the updated_at timestamp of an existing session.
  // Called after each new message turn in the chat loop.
  void touch_session(const std::string& id, int64_t updated_at);

  // Update the title of an existing session.
  void update_session_title(const std::string& id, const std::string& title);

  // Return the session ID whose title matches exactly.
  // Throws std::runtime_error if no match or more than one match found.
  std::string find_session_by_title(const std::string& title);

  // Return the session ID whose id starts with prefix.
  // Throws std::runtime_error if no match or more than one match found.
  std::string find_session_by_id_prefix(const std::string& prefix);

  bool is_open() const;

 private:
  // Opens the file at path_ if not already open.
  // Throws std::runtime_error on failure.
  void open();

  // Encrypt str with key_.
  std::string enc(const std::string& str) const;
  // Decrypt str with key_.
  std::string dec(const std::string& str) const;

  std::filesystem::path path_;
  std::string key_;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> handle_{nullptr,
                                                             sqlite3_close};
};

}  // namespace ur
