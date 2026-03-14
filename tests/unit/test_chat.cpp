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
    ++call_count;
    last_messages = messages;
    if (token_cb) token_cb(response_);
  }

  ur::ServerInfo server_info() override { return {}; }

  int call_count = 0;
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

  // Returns next queued input; returns "" (Ctrl-C) when queue is exhausted.
  std::string read_input() override {
    if (inputs_.empty()) return "";
    std::string line = inputs_.front();
    inputs_.pop();
    return line;
  }

  void print_user(const std::string& content) override {
    user_messages.push_back(content);
  }

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

  void print_error(const std::string& msg) override { errors.push_back(msg); }

  void set_status(int /*prompt_tokens*/, int /*ctx_len*/,
                  const std::string& /*hint*/) override {}

  void start_spinner() override {}
  void stop_spinner() override {}

  std::string system_prompt() const override { return {}; }
  void set_system_prompt(const std::string& /*prompt*/) override {}

  bool is_interactive() const override { return false; }

  std::vector<std::string> user_messages;
  std::vector<std::string> responses;
  std::vector<std::string> chunks;
  std::vector<std::string> reasonings;
  std::vector<std::string> reasoning_chunks;
  std::vector<std::string> errors;

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
  std::queue<std::string> inputs;
  inputs.push("hello world");
  MockProvider mock("response");
  MockTui tui(std::move(inputs));
  ur::Chat chat(*db_, *logger_);
  ur::ChatOptions opts;
  chat.run(opts, mock, tui);
  EXPECT_EQ(db_->select_sessions().size(), 1u);
}

// --continue=<id> resumes a previous session; prior messages appear in context.
TEST_F(ChatTest, ContinueSessionLoadsHistory) {
  // Pre-populate a session with one exchange.
  db_->insert_session("sess1", "", "model", 1000, 1000);
  db_->insert_message("m1", "sess1", "user", "prior question", 1001);
  db_->insert_message("m2", "sess1", "assistant", "prior answer", 1002);

  std::queue<std::string> inputs;
  inputs.push("new question");
  MockProvider mock("new answer");
  MockTui tui(std::move(inputs));
  ur::Chat chat(*db_, *logger_);
  ur::ChatOptions opts;
  opts.continue_id = "sess1";
  chat.run(opts, mock, tui);

  // Prior messages should have been replayed to the TUI.
  ASSERT_GE(tui.user_messages.size(), 1u);
  EXPECT_EQ(tui.user_messages[0], "prior question");
  ASSERT_GE(tui.chunks.size(),
            1u);  // streamed "prior answer" via print_response? No —
  // print_response (non-streaming) is used for replay; responses captures it.
  ASSERT_GE(tui.responses.size(), 1u);
  EXPECT_EQ(tui.responses[0], "prior answer");

  // Provider should have seen the full history in its context window.
  // last_messages contains: prior user, prior assistant, new user (no system).
  const auto& msgs = mock.last_messages;
  ASSERT_GE(msgs.size(), 3u);
  EXPECT_EQ(msgs[msgs.size() - 3].role, "user");
  EXPECT_EQ(msgs[msgs.size() - 3].content, "prior question");
  EXPECT_EQ(msgs[msgs.size() - 2].role, "assistant");
  EXPECT_EQ(msgs[msgs.size() - 1].role, "user");
  EXPECT_EQ(msgs[msgs.size() - 1].content, "new question");
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
  std::string reasoning;
  std::string content = ur::Chat::strip_think(
      "<think>think block</think>actual content", reasoning);
  EXPECT_EQ(content, "actual content");
  EXPECT_EQ(reasoning, "think block");
}

// Stripped <think> block is stored as role "reason" in the database.
TEST_F(ChatTest, ThinkBlockStoredAsReasonRole) {
  std::queue<std::string> inputs;
  inputs.push("hi");
  MockProvider mock("<think>my reasoning</think>hello");
  MockTui tui(std::move(inputs));
  ur::Chat chat(*db_, *logger_);
  ur::ChatOptions opts;
  chat.run(opts, mock, tui);

  auto msgs = db_->select_messages(db_->select_sessions()[0].id);
  auto it = std::find_if(msgs.begin(), msgs.end(), [](const ur::MessageRow& m) {
    return m.role == "reason";
  });
  ASSERT_NE(it, msgs.end());
  EXPECT_EQ(it->content, "my reasoning");
}

// Cleaned assistant content (no <think>) is stored as role "assistant".
TEST_F(ChatTest, AssistantContentStoredClean) {
  std::queue<std::string> inputs;
  inputs.push("hi");
  MockProvider mock("<think>my reasoning</think>hello");
  MockTui tui(std::move(inputs));
  ur::Chat chat(*db_, *logger_);
  ur::ChatOptions opts;
  chat.run(opts, mock, tui);

  auto msgs = db_->select_messages(db_->select_sessions()[0].id);
  auto it = std::find_if(msgs.begin(), msgs.end(), [](const ur::MessageRow& m) {
    return m.role == "assistant";
  });
  ASSERT_NE(it, msgs.end());
  EXPECT_EQ(it->content, "hello");
}

// /exit input terminates the loop without calling the provider.
TEST_F(ChatTest, SlashExitTerminatesLoop) {
  std::queue<std::string> inputs;
  inputs.push("/exit");
  MockProvider mock("response");
  MockTui tui(std::move(inputs));
  ur::Chat chat(*db_, *logger_);
  ur::ChatOptions opts;
  chat.run(opts, mock, tui);
  EXPECT_EQ(mock.call_count, 0);
}

// build_window caps the context to the specified window size.
TEST_F(ChatTest, BuildWindowCapsAtWindowSize) {
  std::vector<ur::Message> history;
  for (int i = 0; i < 30; ++i) {
    history.push_back({"user", "msg"});
    history.push_back({"assistant", "resp"});
  }
  auto window = ur::Chat::build_window(history, "", 10);
  EXPECT_EQ(window.size(), 10u);
}

// build_window excludes "reason" role messages from the provider context.
TEST_F(ChatTest, BuildWindowExcludesReasonRole) {
  std::vector<ur::Message> history = {
      {"user", "hello"},
      {"reason", "thinking..."},
      {"assistant", "hi"},
  };
  auto window = ur::Chat::build_window(history, "", 20);
  EXPECT_EQ(window.size(), 2u);
  for (const auto& m : window) EXPECT_NE(m.role, "reason");
}

// build_window prepends a system message when system_prompt is non-empty.
TEST_F(ChatTest, BuildWindowPrependsSystemMessage) {
  std::vector<ur::Message> history = {
      {"user", "hello"},
      {"assistant", "hi"},
  };
  auto window = ur::Chat::build_window(history, "be helpful", 20);
  ASSERT_EQ(window.size(), 3u);
  EXPECT_EQ(window[0].role, "system");
  EXPECT_EQ(window[0].content, "be helpful");
}
