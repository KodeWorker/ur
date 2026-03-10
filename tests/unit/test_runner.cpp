#include <gtest/gtest.h>
#include <sqlite3.h>

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

namespace fs = std::filesystem;

const std::string kTestKey(32, 'k');

// Mock provider — returns a canned response and records received messages.
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
    root_ = fs::temp_directory_path() /
            ("ur_test_runner_" + std::to_string(::getpid()));
    fs::create_directories(root_);
    db_ = std::make_unique<ur::Database>(root_ / "ur.db", kTestKey);
    db_->init_schema();
    logger_ = std::make_unique<ur::Logger>(root_);
  }

  void TearDown() override {
    db_.reset();
    fs::remove_all(root_);
  }

  fs::path root_;
  std::unique_ptr<ur::Database> db_;
  std::unique_ptr<ur::Logger> logger_;
};

TEST_F(RunnerTest, RunReturnsProviderResponse) {
  MockProvider mock("hello from llm");
  ur::Runner runner(*db_, *logger_);
  auto result = runner.run("hi", "", "", mock);
  EXPECT_EQ(result.response, "hello from llm");
}

TEST_F(RunnerTest, RunPersistsSessionRow) {
  MockProvider mock("response");
  ur::Runner runner(*db_, *logger_);
  auto result = runner.run("hi", "", "", mock);

  sqlite3* handle = nullptr;
  ASSERT_EQ(sqlite3_open((root_ / "ur.db").string().c_str(), &handle),
            SQLITE_OK);

  auto cb = [](void* data, int, char** argv, char**) -> int {
    auto* p = static_cast<std::pair<int, std::string>*>(data);
    p->first++;
    p->second = argv[0];
    return 0;
  };
  std::pair<int, std::string> result_data{0, ""};
  ASSERT_EQ(sqlite3_exec(handle, "SELECT id FROM session;", cb, &result_data,
                         nullptr),
            SQLITE_OK);
  sqlite3_close(handle);

  EXPECT_EQ(result_data.first, 1);
  EXPECT_EQ(result_data.second, result.session_id);
}

TEST_F(RunnerTest, RunPersistsTwoMessageRows) {
  MockProvider mock("response");
  ur::Runner runner(*db_, *logger_);
  runner.run("hi", "", "", mock);

  sqlite3* handle = nullptr;
  ASSERT_EQ(sqlite3_open((root_ / "ur.db").string().c_str(), &handle),
            SQLITE_OK);

  int count = 0;
  auto cb = [](void* data, int, char** argv, char**) -> int {
    *static_cast<int*>(data) = std::stoi(argv[0]);
    return 0;
  };
  ASSERT_EQ(sqlite3_exec(handle, "SELECT count(*) FROM message;", cb, &count,
                         nullptr),
            SQLITE_OK);
  sqlite3_close(handle);

  EXPECT_EQ(count, 2);
}

TEST_F(RunnerTest, RunPassesSystemPromptAsFirstMessage) {
  MockProvider mock("response");
  ur::Runner runner(*db_, *logger_);
  runner.run("hi", "you are helpful", "", mock);

  ASSERT_GE(mock.last_messages.size(), 2u);
  EXPECT_EQ(mock.last_messages[0].role, "system");
  EXPECT_EQ(mock.last_messages[0].content, "you are helpful");
  EXPECT_EQ(mock.last_messages[1].role, "user");
  EXPECT_EQ(mock.last_messages[1].content, "hi");
}

TEST_F(RunnerTest, RunEncryptsContentWithKey) {
  const std::string key(32, 'k');
  auto db_enc = std::make_unique<ur::Database>(root_ / "ur_enc.db", key);
  db_enc->init_schema();
  MockProvider mock("response");
  ur::Runner runner(*db_enc, *logger_);
  runner.run("secret", "", "", mock);
  db_enc.reset();  // close before opening raw handle

  sqlite3* handle = nullptr;
  ASSERT_EQ(sqlite3_open((root_ / "ur_enc.db").string().c_str(), &handle),
            SQLITE_OK);

  // Read raw content blob of the user message.
  std::string raw_content;
  auto cb = [](void* data, int, char** argv, char**) -> int {
    if (argv[0]) *static_cast<std::string*>(data) = argv[0];
    return 0;
  };
  ASSERT_EQ(
      sqlite3_exec(handle, "SELECT content FROM message WHERE role='user';", cb,
                   &raw_content, nullptr),
      SQLITE_OK);
  sqlite3_close(handle);

  // Raw stored bytes must not equal the plaintext prompt.
  EXPECT_NE(raw_content, "secret");
}
