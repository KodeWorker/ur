#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "agent/runner.hpp"
#include "llm/provider.hpp"
#include "log/logger.hpp"
#include "memory/database.hpp"

namespace {

// Mock provider that returns a canned response without any network call.
class MockProvider : public ur::Provider {
 public:
  explicit MockProvider(std::string response)
      : response_(std::move(response)) {}

  std::string complete(const std::vector<ur::Message>& messages,
                       const std::string& /*model*/) override {
    last_messages = messages;
    return response_;
  }

  std::vector<ur::Message> last_messages;

 private:
  std::string response_;
};

}  // namespace

class RunnerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // TODO: create a unique temp directory for the database file.
    // db_path = std::filesystem::temp_directory_path() / "ur_test_runner_XXXX"
    //           / "ur.db";
    // std::filesystem::create_directories(db_path.parent_path());
    // db    = std::make_unique<ur::Database>(db_path);
    // logger = std::make_unique<ur::Logger>(db_path.parent_path());
  }

  void TearDown() override {
    // TODO: remove temp directory and all contents.
    // db.reset();
    // std::filesystem::remove_all(db_path.parent_path());
  }

  std::filesystem::path db_path;
  std::unique_ptr<ur::Database> db;
  std::unique_ptr<ur::Logger> logger;
};

TEST_F(RunnerTest, RunReturnsProviderResponse) {
  // TODO:
  // 1. MockProvider mock("hello from llm").
  // 2. Runner runner(*db, "", *logger).
  // 3. auto result = runner.run("hi", "", "", mock).
  // 4. EXPECT_EQ(result.response, "hello from llm").
  GTEST_SKIP() << "not implemented";
}

TEST_F(RunnerTest, RunPersistsSessionRow) {
  // TODO:
  // 1. run a request.
  // 2. Query session table; assert one row exists with the returned session_id.
  GTEST_SKIP() << "not implemented";
}

TEST_F(RunnerTest, RunPersistsTwoMessageRows) {
  // TODO:
  // 1. run a request.
  // 2. Query message table; assert two rows (user + assistant).
  GTEST_SKIP() << "not implemented";
}

TEST_F(RunnerTest, RunPassesSystemPromptAsFirstMessage) {
  // TODO:
  // 1. MockProvider that captures messages.
  // 2. runner.run("hi", "you are helpful", "", mock).
  // 3. Assert mock.last_messages[0].role == "system".
  // 4. Assert mock.last_messages[0].content == "you are helpful".
  GTEST_SKIP() << "not implemented";
}

TEST_F(RunnerTest, RunEncryptsContentWithKey) {
  // TODO:
  // 1. Generate a 32-byte key string.
  // 2. Runner runner(*db, key, *logger).
  // 3. runner.run("secret", "", "", mock).
  // 4. Query message table raw content; assert it is NOT "secret" (encrypted).
  GTEST_SKIP() << "not implemented";
}
