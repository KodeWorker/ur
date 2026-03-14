#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "agent/persona_updater.hpp"
#include "log/logger.hpp"
#include "memory/database.hpp"

namespace {

namespace fs = std::filesystem;

const std::string kKey(32, 'k');

// ---------------------------------------------------------------------------
// MockProvider — counts calls and returns a canned response
// ---------------------------------------------------------------------------

class MockProvider : public ur::Provider {
 public:
  explicit MockProvider(std::string response)
      : response_(std::move(response)) {}

  ur::CompletionResult complete(const std::vector<ur::Message>& messages,
                                const std::string& /*model*/) override {
    ++call_count;
    last_messages = messages;
    return {response_, {}, 0, 0};
  }

  void stream(const std::vector<ur::Message>& /*messages*/,
              const std::string& /*model*/, const ur::TokenCallback& token_cb,
              const ur::TokenCallback& /*reasoning_cb*/) override {
    ++call_count;
    if (token_cb) token_cb(response_);
  }

  ur::ServerInfo server_info() override { return {}; }

  int call_count = 0;
  std::vector<ur::Message> last_messages;

 private:
  std::string response_;
};

// Build a context with `n` complete user+assistant exchanges.
static std::vector<ur::Message> make_context(int exchanges) {
  std::vector<ur::Message> ctx;
  for (int i = 0; i < exchanges; ++i) {
    ctx.push_back({"user", std::string(60, 'x')});
    ctx.push_back({"assistant", "response"});
  }
  return ctx;
}

}  // namespace

class PersonaUpdaterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("ur_test_persona_" + std::to_string(::getpid()));
    fs::create_directories(root_);
    db_ = std::make_unique<ur::Database>(root_ / "ur.db", kKey);
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

// Short user message (<= 50 chars) does not trigger extraction.
TEST_F(PersonaUpdaterTest, ShortMessageSkipsUpdate) {
  MockProvider mock("{}");
  ur::PersonaUpdater updater(*db_, mock, *logger_, "");
  auto ctx = make_context(3);
  updater.maybe_update(ctx, "hi");
  EXPECT_EQ(mock.call_count, 0);
}

// Shallow context (< 6 user+assistant messages) does not trigger extraction.
TEST_F(PersonaUpdaterTest, ShallowContextSkipsUpdate) {
  MockProvider mock("{}");
  ur::PersonaUpdater updater(*db_, mock, *logger_, "");
  // Only 2 exchanges = 4 messages, below the threshold of 6.
  auto ctx = make_context(2);
  std::string long_msg(60, 'x');
  updater.maybe_update(ctx, long_msg);
  EXPECT_EQ(mock.call_count, 0);
}

// Meaningful turn (long message + deep context) calls the provider once.
TEST_F(PersonaUpdaterTest, MeaningfulTurnCallsProvider) {
  MockProvider mock("{}");
  ur::PersonaUpdater updater(*db_, mock, *logger_, "");
  auto ctx = make_context(4);  // 8 messages ≥ 6 threshold
  std::string long_msg(60, 'x');
  updater.maybe_update(ctx, long_msg);
  EXPECT_EQ(mock.call_count, 1);
}

// Extracted key-value pairs are upserted into the persona table.
TEST_F(PersonaUpdaterTest, ExtractedFactUpsertedToDb) {
  MockProvider mock("{\"name\": \"Alice\", \"timezone\": \"UTC\"}");
  ur::PersonaUpdater updater(*db_, mock, *logger_, "");
  auto ctx = make_context(4);
  std::string long_msg(60, 'x');
  updater.maybe_update(ctx, long_msg);

  auto rows = db_->select_persona();
  ASSERT_EQ(rows.size(), 2u);
  // select_persona returns rows ordered by key ASC: name, timezone.
  EXPECT_EQ(rows[0].key, "name");
  EXPECT_EQ(rows[0].value, "Alice");
  EXPECT_EQ(rows[1].key, "timezone");
  EXPECT_EQ(rows[1].value, "UTC");
}

// A second extraction for the same key overwrites the previous value.
TEST_F(PersonaUpdaterTest, UpsertOverwritesExistingKey) {
  auto ctx = make_context(4);
  std::string long_msg(60, 'x');

  MockProvider mock1("{\"name\": \"Alice\"}");
  ur::PersonaUpdater updater1(*db_, mock1, *logger_, "");
  updater1.maybe_update(ctx, long_msg);

  MockProvider mock2("{\"name\": \"Bob\"}");
  ur::PersonaUpdater updater2(*db_, mock2, *logger_, "");
  updater2.maybe_update(ctx, long_msg);

  auto rows = db_->select_persona();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].value, "Bob");
}
