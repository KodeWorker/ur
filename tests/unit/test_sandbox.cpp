#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "tools/sandbox.hpp"

namespace {

namespace fs = std::filesystem;

// Helper: build a SandboxPolicy with a given workspace root.
ur::SandboxPolicy make_policy(const fs::path& workspace,
                              bool allow_all = false) {
  return {workspace, allow_all};
}

}  // namespace

class SandboxTest : public ::testing::Test {
 protected:
  void SetUp() override {
    workspace_ = fs::temp_directory_path() /
                 ("ur_test_sandbox_" + std::to_string(::getpid()));
    fs::create_directories(workspace_);
  }
  void TearDown() override { fs::remove_all(workspace_); }

  fs::path workspace_;
};

// ---------------------------------------------------------------------------
// Path enforcement
// ---------------------------------------------------------------------------

TEST_F(SandboxTest, PathInsideWorkspaceIsAllowed) {
  GTEST_SKIP() << "TODO: implement Sandbox::validate";
  auto err =
      ur::Sandbox::validate(workspace_ / "file.txt", make_policy(workspace_));
  EXPECT_TRUE(err.empty());
}

TEST_F(SandboxTest, PathOutsideWorkspaceIsRejected) {
  GTEST_SKIP() << "TODO: implement Sandbox::validate";
  auto err = ur::Sandbox::validate("/etc/passwd", make_policy(workspace_));
  EXPECT_FALSE(err.empty());
}

TEST_F(SandboxTest, DotDotTraversalIsRejected) {
  GTEST_SKIP() << "TODO: implement Sandbox::validate";
  auto err = ur::Sandbox::validate(workspace_ / ".." / "escape.txt",
                                   make_policy(workspace_));
  EXPECT_FALSE(err.empty());
}

TEST_F(SandboxTest, AllowAllBypasses) {
  GTEST_SKIP() << "TODO: implement Sandbox::validate";
  // With allow_all=true even /etc/passwd is permitted.
  auto err =
      ur::Sandbox::validate("/etc/passwd", make_policy(workspace_, true));
  EXPECT_TRUE(err.empty());
}

TEST_F(SandboxTest, WorkspaceRootItselfIsAllowed) {
  GTEST_SKIP() << "TODO: implement Sandbox::validate";
  auto err = ur::Sandbox::validate(workspace_, make_policy(workspace_));
  EXPECT_TRUE(err.empty());
}

TEST_F(SandboxTest, NestedSubdirIsAllowed) {
  GTEST_SKIP() << "TODO: implement Sandbox::validate";
  auto err = ur::Sandbox::validate(workspace_ / "a" / "b" / "c.txt",
                                   make_policy(workspace_));
  EXPECT_TRUE(err.empty());
}
