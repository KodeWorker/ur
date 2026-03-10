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
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, SessionExistsReturnsFalseForUnknownId) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, SessionExistsReturnsTrueAfterInsert) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, SelectMessagesReturnsRowsInOrder) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, SelectMessagesDecryptsContent) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, SelectPersonaReturnsEmptyWhenNone) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, UpsertPersonaInsertsNewEntry) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, UpsertPersonaOverwritesExistingEntry) {
  // TODO
  GTEST_SKIP();
}

TEST_F(DatabaseTest, TouchSessionUpdatesTimestamp) {
  // TODO
  GTEST_SKIP();
}
