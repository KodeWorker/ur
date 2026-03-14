#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "log/logger.hpp"
#include "memory/database.hpp"
#include "tools/executor.hpp"
#include "tools/loader.hpp"
#include "tools/tool.hpp"

namespace {

namespace fs = std::filesystem;

const std::string kKey(32, 'k');

// ---------------------------------------------------------------------------
// MockTool — controllable Tool stub for Executor tests.
// ---------------------------------------------------------------------------
class MockTool : public ur::Tool {
 public:
  explicit MockTool(std::string name, bool requires_allow_all = false,
                    ur::ToolResult result = {"ok", false})
      : name_(std::move(name)),
        requires_allow_all_(requires_allow_all),
        result_(std::move(result)) {}

  std::string name() const override { return name_; }
  std::string description() const override { return "mock"; }
  std::string input_schema_json() const override { return "{}"; }
  bool requires_allow_all() const override { return requires_allow_all_; }

  ur::ToolResult execute(const std::string& /*args_json*/,
                         const ur::SandboxPolicy& /*policy*/) override {
    call_count++;
    return result_;
  }

  int call_count = 0;

 private:
  std::string name_;
  bool requires_allow_all_;
  ur::ToolResult result_;
};

}  // namespace

class ExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("ur_test_executor_" + std::to_string(::getpid()));
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

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(ExecutorTest, UnknownToolReturnsError) {
  GTEST_SKIP() << "TODO: implement Executor::execute_all";
  // Loader with no tools registered.
  // ur::Loader loader(paths_);
  // ur::SandboxPolicy policy{root_ / "workspace", true};
  // ur::Executor executor(loader, policy, *db_, *logger_);
  //
  // ur::ToolCall call{"id1", "nonexistent", "{}"};
  // auto results = executor.execute_all({call}, "session1",
  //                                     [](const ur::ToolCall&){ return true;
  //                                     });
  // ASSERT_EQ(results.size(), 1u);
  // EXPECT_TRUE(results[0].is_error);
}

TEST_F(ExecutorTest, ApprovedCallExecutesTool) {
  GTEST_SKIP() << "TODO: implement Executor::execute_all";
  // auto* mock = new MockTool("read_file");
  // Register mock in loader, set audit_cb to return true.
  // Verify mock->call_count == 1 and result is not an error.
}

TEST_F(ExecutorTest, RejectedCallSkipsExecution) {
  GTEST_SKIP() << "TODO: implement Executor::execute_all";
  // audit_cb returns false.
  // Verify mock->call_count == 0 and result is_error == true.
}

TEST_F(ExecutorTest, AllowAllBypassesAudit) {
  GTEST_SKIP() << "TODO: implement Executor::execute_all";
  // policy.allow_all = true.
  // audit_cb should never be called.
  // Verify mock->call_count == 1.
}

TEST_F(ExecutorTest, MultipleCallsReturnResultsInOrder) {
  GTEST_SKIP() << "TODO: implement Executor::execute_all";
  // Two tool calls; verify results vector has same size and order.
}

TEST_F(ExecutorTest, AuditMessagePersistedOnApproval) {
  GTEST_SKIP() << "TODO: implement Executor::persist_audit";
  // After an approved call, query db_ for role="tool_audit" messages.
  // Expect one row containing "approved".
}

TEST_F(ExecutorTest, AuditMessagePersistedOnRejection) {
  GTEST_SKIP() << "TODO: implement Executor::persist_audit";
  // After a rejected call, query db_ for role="tool_audit" messages.
  // Expect one row containing "rejected".
}
