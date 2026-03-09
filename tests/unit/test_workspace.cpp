#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "memory/workspace.hpp"

namespace {

namespace fs = std::filesystem;

// Builds a Paths struct rooted at an arbitrary temp directory.
ur::Paths make_test_paths(const fs::path& root) {
  return {root,           root / "workspace", root / "database",
          root / "tools", root / "log",       root / "keys"};
}

class WorkspaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ =
        fs::temp_directory_path() / ("ur_test_" + std::to_string(::getpid()));
    fs::create_directories(root_);
  }

  void TearDown() override { fs::remove_all(root_); }

  fs::path root_;
};

TEST(WorkspaceResolveTest, ResolvePathsReturnsNonEmptyRoot) {
  ur::Paths paths = ur::resolve_paths();
  EXPECT_FALSE(paths.root.empty());
}

TEST_F(WorkspaceTest, InitWorkspaceCreatesAllSubdirs) {
  ur::Paths paths = make_test_paths(root_);
  ur::init_workspace(paths);

  EXPECT_TRUE(fs::is_directory(paths.workspace));
  EXPECT_TRUE(fs::is_directory(paths.database));
  EXPECT_TRUE(fs::is_directory(paths.tools));
  EXPECT_TRUE(fs::is_directory(paths.logs));
  EXPECT_TRUE(fs::is_directory(paths.keys));
}

TEST_F(WorkspaceTest, InitWorkspaceIsIdempotent) {
  ur::Paths paths = make_test_paths(root_);
  EXPECT_NO_THROW(ur::init_workspace(paths));
  EXPECT_NO_THROW(ur::init_workspace(paths));
}

TEST_F(WorkspaceTest, RemoveWorkspaceClearsContents) {
  ur::Paths paths = make_test_paths(root_);
  ur::init_workspace(paths);

  // Create a file inside workspace/.
  std::ofstream(paths.workspace / "sentinel.txt") << "data";
  ASSERT_TRUE(fs::exists(paths.workspace / "sentinel.txt"));

  ur::remove_workspace(paths);

  EXPECT_FALSE(fs::exists(paths.workspace / "sentinel.txt"));
}

}  // namespace
