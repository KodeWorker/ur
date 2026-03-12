#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "agent/chat.hpp"
#include "cli/tui.hpp"
#include "log/logger.hpp"
#include "memory/database.hpp"

namespace {

namespace fs = std::filesystem;

const std::string kKey(32, 'k');

// ---------------------------------------------------------------------------
// MockProvider
// ---------------------------------------------------------------------------

class MockProvider : public ur::Provider {
 public:
  explicit MockProvider(std::string response)
      : response_(std::move(response)) {}

  ur::CompletionResult complete(const std::vector<ur::Message>& messages,
                                const std::string& /*model*/) override {
    last_messages = messages;
    return {response_, {}, 0, 0};
  }

  void stream(const std::vector<ur::Message>& messages,
              const std::string& /*model*/, const ur::TokenCallback& token_cb,
              const ur::TokenCallback& /*reasoning_cb*/) override {
    last_messages = messages;
    // Deliver the whole response as one chunk.
    if (token_cb) token_cb(response_);
  }

  ur::ServerInfo server_info() override { return {}; }

  std::vector<ur::Message> last_messages;

 private:
  std::string response_;
};

// ---------------------------------------------------------------------------
// MockTui — feeds scripted input, records output
// ---------------------------------------------------------------------------

class MockTui : public ur::Tui {
 public:
  explicit MockTui(std::queue<std::string> inputs)
      : inputs_(std::move(inputs)) {}

  // Returns next queued input; returns "/exit" when queue is exhausted.
  std::string read_input() override {
    if (inputs_.empty()) return "/exit";
    std::string line = inputs_.front();
    inputs_.pop();
    return line;
  }

  void print_user(const std::string& /*content*/) override {}

  void print_response(const std::string& content) override {
    responses.push_back(content);
  }

  void print_response_chunk(const std::string& chunk) override {
    chunks.push_back(chunk);
  }

  void print_reasoning(const std::string& reasoning) override {
    reasonings.push_back(reasoning);
  }

  void print_reasoning_chunk(const std::string& chunk) override {
    reasoning_chunks.push_back(chunk);
  }

  void print_error(const std::string& /*msg*/) override {}

  void set_status(int /*prompt_tokens*/, int /*ctx_len*/,
                  const std::string& /*hint*/) override {}

  void start_spinner() override {}
  void stop_spinner() override {}

  std::string system_prompt() const override { return {}; }

  std::vector<std::string> responses;
  std::vector<std::string> chunks;
  std::vector<std::string> reasonings;
  std::vector<std::string> reasoning_chunks;

 private:
  std::queue<std::string> inputs_;
};

}  // namespace

class ChatTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("ur_test_chat_" + std::to_string(::getpid()));
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

// Sending one message then /exit creates exactly one session row.
TEST_F(ChatTest, NewSessionCreatesSessionRow) {
  // TODO
  GTEST_SKIP();
}

// --continue=<id> resumes a previous session; prior messages appear in context.
TEST_F(ChatTest, ContinueSessionLoadsHistory) {
  // TODO
  GTEST_SKIP();
}

// strip_think returns raw unchanged and leaves reasoning_out empty when there
// is no <think> tag.
TEST_F(ChatTest, StripThinkNoTagReturnsRaw) {
  std::string reasoning;
  EXPECT_EQ(ur::Chat::strip_think("hello", reasoning), "hello");
  EXPECT_TRUE(reasoning.empty());
}

// strip_think extracts the reasoning block and returns cleaned content.
TEST_F(ChatTest, StripThinkExtractsReasoningBlock) {
  // TODO
  GTEST_SKIP();
}

// Stripped <think> block is stored as role "reason" in the database.
TEST_F(ChatTest, ThinkBlockStoredAsReasonRole) {
  // TODO
  GTEST_SKIP();
}

// Cleaned assistant content (no <think>) is stored as role "assistant".
TEST_F(ChatTest, AssistantContentStoredClean) {
  // TODO
  GTEST_SKIP();
}

// /exit input terminates the loop without calling the provider.
TEST_F(ChatTest, SlashExitTerminatesLoop) {
  // TODO
  GTEST_SKIP();
}

// build_window caps the context to the specified window size.
TEST_F(ChatTest, BuildWindowCapsAtWindowSize) {
  std::vector<ur::Message> history;
  for (int i = 0; i < 30; ++i) {
    history.push_back({"user", "msg"});
    history.push_back({"assistant", "resp"});
  }
  auto window = ur::Chat::build_window(history, "", 10);
  // TODO: EXPECT_EQ(window.size(), 10u);
  GTEST_SKIP();
}

// build_window excludes "reason" role messages from the provider context.
TEST_F(ChatTest, BuildWindowExcludesReasonRole) {
  // TODO
  GTEST_SKIP();
}

// build_window prepends a system message when system_prompt is non-empty.
TEST_F(ChatTest, BuildWindowPrependsSystemMessage) {
  // TODO
  GTEST_SKIP();
}
