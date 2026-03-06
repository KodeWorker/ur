#include <gtest/gtest.h>

#include "memory/workspace.hpp"

// Helper: create a temporary directory unique to this test run.
// Use std::filesystem::temp_directory_path() / a unique subdirectory.

namespace {

// TODO: add a fixture that creates a temp root dir in SetUp()
//       and removes it in TearDown().

TEST(WorkspaceTest, ResolvePathsReturnsNonEmptyRoot) {
  // TODO: call resolve_paths() and assert that paths.root is not empty.
}

TEST(WorkspaceTest, InitWorkspaceCreatesAllSubdirs) {
  // TODO: build a Paths pointing at a temp dir, call init_workspace(),
  //       and assert that workspace/, database/, tools/, log/, keys/ all exist.
}

TEST(WorkspaceTest, InitWorkspaceIsIdempotent) {
  // TODO: call init_workspace() twice on the same Paths and assert no error.
}

TEST(WorkspaceTest, RemoveWorkspaceClearsContents) {
  // TODO: init workspace, create a file inside workspace/,
  //       call remove_workspace(), assert the file no longer exists.
}

}  // namespace
