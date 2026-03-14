#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "tools/builtin/bash.hpp"
#include "tools/builtin/read_file.hpp"
#include "tools/builtin/read_image.hpp"
#include "tools/builtin/web_fetch.hpp"
#include "tools/builtin/web_search.hpp"
#include "tools/builtin/write_file.hpp"
#include "tools/sandbox.hpp"

namespace {

namespace fs = std::filesystem;

ur::SandboxPolicy make_policy(const fs::path& workspace,
                              bool allow_all = false) {
  return {workspace, allow_all};
}

}  // namespace

class BuiltinToolsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    workspace_ = fs::temp_directory_path() /
                 ("ur_test_builtin_" + std::to_string(::getpid()));
    fs::create_directories(workspace_);
  }
  void TearDown() override { fs::remove_all(workspace_); }

  fs::path workspace_;
};

// ---------------------------------------------------------------------------
// read_file
// ---------------------------------------------------------------------------

TEST_F(BuiltinToolsTest, ReadFileReturnsContent) {
  GTEST_SKIP() << "TODO: implement ReadFileTool::execute";
  std::ofstream f(workspace_ / "hello.txt");
  f << "hello world";
  f.close();

  ur::ReadFileTool tool;
  auto result =
      tool.execute(R"({"path":"hello.txt"})", make_policy(workspace_));
  EXPECT_FALSE(result.is_error);
  EXPECT_EQ(result.content, "hello world");
}

TEST_F(BuiltinToolsTest, ReadFileRejectsPathOutsideWorkspace) {
  GTEST_SKIP() << "TODO: implement ReadFileTool::execute";
  ur::ReadFileTool tool;
  auto result =
      tool.execute(R"({"path":"/etc/passwd"})", make_policy(workspace_));
  EXPECT_TRUE(result.is_error);
}

TEST_F(BuiltinToolsTest, ReadFileMissingFileReturnsError) {
  GTEST_SKIP() << "TODO: implement ReadFileTool::execute";
  ur::ReadFileTool tool;
  auto result =
      tool.execute(R"({"path":"nonexistent.txt"})", make_policy(workspace_));
  EXPECT_TRUE(result.is_error);
}

// ---------------------------------------------------------------------------
// write_file
// ---------------------------------------------------------------------------

TEST_F(BuiltinToolsTest, WriteFileCreatesFile) {
  GTEST_SKIP() << "TODO: implement WriteFileTool::execute";
  ur::WriteFileTool tool;
  auto result = tool.execute(R"({"path":"out.txt","content":"data"})",
                             make_policy(workspace_));
  EXPECT_FALSE(result.is_error);
  EXPECT_TRUE(fs::exists(workspace_ / "out.txt"));
}

TEST_F(BuiltinToolsTest, WriteFileCreatesParentDirs) {
  GTEST_SKIP() << "TODO: implement WriteFileTool::execute";
  ur::WriteFileTool tool;
  auto result = tool.execute(R"({"path":"a/b/out.txt","content":"x"})",
                             make_policy(workspace_));
  EXPECT_FALSE(result.is_error);
  EXPECT_TRUE(fs::exists(workspace_ / "a" / "b" / "out.txt"));
}

TEST_F(BuiltinToolsTest, WriteFileRejectsPathOutsideWorkspace) {
  GTEST_SKIP() << "TODO: implement WriteFileTool::execute";
  ur::WriteFileTool tool;
  auto result = tool.execute(R"({"path":"/tmp/evil.txt","content":"x"})",
                             make_policy(workspace_));
  EXPECT_TRUE(result.is_error);
}

// ---------------------------------------------------------------------------
// bash
// ---------------------------------------------------------------------------

TEST_F(BuiltinToolsTest, BashRequiresAllowAll) {
  GTEST_SKIP() << "TODO: implement BashTool::execute";
  ur::BashTool tool;
  // Without allow_all, should return an error without executing.
  auto result =
      tool.execute(R"({"command":"echo hi"})", make_policy(workspace_, false));
  EXPECT_TRUE(result.is_error);
}

TEST_F(BuiltinToolsTest, BashCapturesOutput) {
  GTEST_SKIP() << "TODO: implement BashTool::execute";
  ur::BashTool tool;
  auto result = tool.execute(R"({"command":"echo hello"})",
                             make_policy(workspace_, true));
  EXPECT_FALSE(result.is_error);
  EXPECT_NE(result.content.find("hello"), std::string::npos);
}

TEST_F(BuiltinToolsTest, BashNonZeroExitIsError) {
  GTEST_SKIP() << "TODO: implement BashTool::execute";
  ur::BashTool tool;
  auto result =
      tool.execute(R"({"command":"exit 1"})", make_policy(workspace_, true));
  EXPECT_TRUE(result.is_error);
}

// ---------------------------------------------------------------------------
// read_image
// ---------------------------------------------------------------------------

TEST_F(BuiltinToolsTest, ReadImageReturnsPathForExistingFile) {
  GTEST_SKIP() << "TODO: implement ReadImageTool::execute";
  // Create a dummy image file.
  std::ofstream f(workspace_ / "img.png");
  f << "PNG";
  f.close();

  ur::ReadImageTool tool;
  auto result =
      tool.execute(R"({"path":"img.png"})", make_policy(workspace_, true));
  EXPECT_FALSE(result.is_error);
  EXPECT_NE(result.content.find("img.png"), std::string::npos);
}

TEST_F(BuiltinToolsTest, ReadImageRejectsPathOutsideWorkspace) {
  GTEST_SKIP() << "TODO: implement ReadImageTool::execute";
  ur::ReadImageTool tool;
  auto result =
      tool.execute(R"({"path":"/etc/shadow"})", make_policy(workspace_, true));
  EXPECT_TRUE(result.is_error);
}
