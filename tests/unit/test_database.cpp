#include <gtest/gtest.h>

#include "memory/database.hpp"

// All tests use a temporary database file that is removed after each test.

namespace {

// TODO: add a fixture that builds a temp path in SetUp()
//       and removes the file in TearDown().

TEST(DatabaseTest, IsNotOpenBeforeFirstCall) {
  // TODO: construct a Database, assert is_open() == false.
}

TEST(DatabaseTest, InitSchemaOpensDatabase) {
  // TODO: construct a Database, call init_schema(),
  //       assert is_open() == true.
}

TEST(DatabaseTest, InitSchemaIsIdempotent) {
  // TODO: call init_schema() twice, assert no error.
}

TEST(DatabaseTest, InitSchemaCreatesAllThreeTables) {
  // TODO: call init_schema(), then query sqlite_master to confirm
  //       session, message, and persona tables exist.
}

TEST(DatabaseTest, DropAllRemovesAllTables) {
  // TODO: call init_schema(), then drop_all(), then query sqlite_master
  //       and assert no tables remain.
}

} // namespace
