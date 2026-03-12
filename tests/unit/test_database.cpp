#include <gtest/gtest.h>
#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "memory/database.hpp"

namespace {

namespace fs = std::filesystem;

class DatabaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    db_path_ = fs::temp_directory_path() /
               ("ur_test_db_" + std::to_string(::getpid()) + ".db");
    key_ = std::string(32, 'k');
  }

  void TearDown() override { fs::remove(db_path_); }

  fs::path db_path_;
  std::string key_;
};

TEST_F(DatabaseTest, IsNotOpenBeforeFirstCall) {
  ur::Database db(db_path_, key_);
  EXPECT_FALSE(db.is_open());
}

TEST_F(DatabaseTest, InitSchemaOpensDatabase) {
  ur::Database db(db_path_, key_);
  EXPECT_NO_THROW(db.init_schema());
  EXPECT_TRUE(db.is_open());
}

TEST_F(DatabaseTest, InitSchemaIsIdempotent) {
  ur::Database db(db_path_, key_);
  EXPECT_NO_THROW(db.init_schema());
  EXPECT_NO_THROW(db.init_schema());
}

TEST_F(DatabaseTest, InitSchemaCreatesAllThreeTables) {
  ur::Database db(db_path_, key_);
  db.init_schema();

  // Query sqlite_master for table names.
  sqlite3* handle = nullptr;
  ASSERT_EQ(sqlite3_open(db_path_.string().c_str(), &handle), SQLITE_OK);

  const char* sql =
      "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;";
  std::vector<std::string> tables;
  auto cb = [](void* data, int, char** argv, char**) -> int {
    static_cast<std::vector<std::string>*>(data)->emplace_back(argv[0]);
    return 0;
  };
  ASSERT_EQ(sqlite3_exec(handle, sql, cb, &tables, nullptr), SQLITE_OK);
  sqlite3_close(handle);

  EXPECT_EQ(tables.size(), 3u);
  EXPECT_NE(std::find(tables.begin(), tables.end(), "session"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "message"), tables.end());
  EXPECT_NE(std::find(tables.begin(), tables.end(), "persona"), tables.end());
}

TEST_F(DatabaseTest, DropAllRemovesAllTables) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  EXPECT_NO_THROW(db.drop_all());

  sqlite3* handle = nullptr;
  ASSERT_EQ(sqlite3_open(db_path_.string().c_str(), &handle), SQLITE_OK);

  const char* sql = "SELECT count(*) FROM sqlite_master WHERE type='table';";
  int count = -1;
  auto cb = [](void* data, int, char** argv, char**) -> int {
    *static_cast<int*>(data) = std::stoi(argv[0]);
    return 0;
  };
  ASSERT_EQ(sqlite3_exec(handle, sql, cb, &count, nullptr), SQLITE_OK);
  sqlite3_close(handle);

  EXPECT_EQ(count, 0);
}

}  // namespace

// ---------------------------------------------------------------------------
// Phase 3 — select / upsert / touch
// ---------------------------------------------------------------------------

TEST_F(DatabaseTest, SelectSessionsReturnsEmptyWhenNone) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  EXPECT_TRUE(db.select_sessions().empty());
}

TEST_F(DatabaseTest, SessionExistsReturnsFalseForUnknownId) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  EXPECT_FALSE(db.session_exists("nonexistent"));
}

TEST_F(DatabaseTest, SessionExistsReturnsTrueAfterInsert) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  EXPECT_TRUE(db.session_exists("s1"));
}

TEST_F(DatabaseTest, SelectMessagesReturnsRowsInOrder) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  db.insert_message("m1", "s1", "user", "first", 1001);
  db.insert_message("m2", "s1", "assistant", "second", 1002);
  db.insert_message("m3", "s1", "user", "third", 1003);

  auto msgs = db.select_messages("s1");
  ASSERT_EQ(msgs.size(), 3u);
  EXPECT_EQ(msgs[0].content, "first");
  EXPECT_EQ(msgs[1].content, "second");
  EXPECT_EQ(msgs[2].content, "third");
}

TEST_F(DatabaseTest, SelectMessagesDecryptsContent) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  db.insert_message("m1", "s1", "user", "hello world", 1001);

  auto msgs = db.select_messages("s1");
  ASSERT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].content, "hello world");
}

TEST_F(DatabaseTest, SelectPersonaReturnsEmptyWhenNone) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  EXPECT_TRUE(db.select_persona().empty());
}

TEST_F(DatabaseTest, UpsertPersonaInsertsNewEntry) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.upsert_persona("name", "Alice", 1000);

  auto rows = db.select_persona();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].key, "name");
  EXPECT_EQ(rows[0].value, "Alice");
}

TEST_F(DatabaseTest, UpsertPersonaOverwritesExistingEntry) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.upsert_persona("name", "Alice", 1000);
  db.upsert_persona("name", "Bob", 2000);

  auto rows = db.select_persona();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].value, "Bob");
  EXPECT_EQ(rows[0].updated_at, 2000);
}

TEST_F(DatabaseTest, TouchSessionUpdatesTimestamp) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  db.touch_session("s1", 2000);

  auto sessions = db.select_sessions();
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].updated_at, 2000);
}

TEST_F(DatabaseTest, UpdateSessionTitleSetsTitle) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  db.update_session_title("s1", "my chat");

  auto sessions = db.select_sessions();
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].title, "my chat");
}

TEST_F(DatabaseTest, FindSessionByTitleReturnsId) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  db.update_session_title("s1", "my chat");

  EXPECT_EQ(db.find_session_by_title("my chat"), "s1");
}

TEST_F(DatabaseTest, FindSessionByTitleThrowsWhenNotFound) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  EXPECT_THROW(db.find_session_by_title("ghost"), std::runtime_error);
}

TEST_F(DatabaseTest, FindSessionByIdPrefixReturnsFullId) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("abcdef1234567890abcdef1234567890", "", "model", 1000,
                    1000);

  EXPECT_EQ(db.find_session_by_id_prefix("abcdef"),
            "abcdef1234567890abcdef1234567890");
}

TEST_F(DatabaseTest, FindSessionByIdPrefixThrowsOnAmbiguous) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("abcdef1111111111111111111111111a", "", "model", 1000,
                    1000);
  db.insert_session("abcdef2222222222222222222222222b", "", "model", 1001,
                    1001);

  EXPECT_THROW(db.find_session_by_id_prefix("abcdef"), std::runtime_error);
}

TEST_F(DatabaseTest, SelectSessionsOrderedByCreatedAtDesc) {
  ur::Database db(db_path_, key_);
  db.init_schema();
  db.insert_session("s1", "", "model", 1000, 1000);
  db.insert_session("s2", "", "model", 2000, 2000);
  db.insert_session("s3", "", "model", 1500, 1500);

  auto sessions = db.select_sessions();
  ASSERT_EQ(sessions.size(), 3u);
  EXPECT_EQ(sessions[0].id, "s2");
  EXPECT_EQ(sessions[1].id, "s3");
  EXPECT_EQ(sessions[2].id, "s1");
}
