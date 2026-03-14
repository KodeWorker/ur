#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "cli/context.hpp"
#include "tools/loader.hpp"

namespace {

namespace fs = std::filesystem;

// Build a minimal Paths struct pointing at a temp directory.
ur::Paths make_paths(const fs::path& root) {
  ur::Paths p;
  p.workspace = root / "workspace";
  p.tools = root / "tools";
  p.database = root / "database";
  p.log = root / "log";
  p.keys = root / "keys";
  return p;
}

// Write a minimal tools.json to the tools directory.
void write_manifest(const fs::path& tools_dir, const std::string& json) {
  fs::create_directories(tools_dir);
  std::ofstream f(tools_dir / "tools.json");
  f << json;
}

}  // namespace

class LoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("ur_test_loader_" + std::to_string(::getpid()));
    fs::create_directories(root_ / "workspace");
    fs::create_directories(root_ / "tools");
    paths_ = make_paths(root_);
  }
  void TearDown() override { fs::remove_all(root_); }

  fs::path root_;
  ur::Paths paths_;
};

// ---------------------------------------------------------------------------
// Built-in registration
// ---------------------------------------------------------------------------

TEST_F(LoaderTest, BuiltinsRegisteredAfterLoad) {
  GTEST_SKIP() << "TODO: implement Loader::register_builtins";
  ur::Loader loader(paths_);
  loader.load("", "", false, true);
  EXPECT_NE(loader.find("read_file"), nullptr);
  EXPECT_NE(loader.find("write_file"), nullptr);
  EXPECT_NE(loader.find("bash"), nullptr);
}

TEST_F(LoaderTest, FindReturnsNullptrForUnknownTool) {
  GTEST_SKIP() << "TODO: implement Loader::register_builtins";
  ur::Loader loader(paths_);
  loader.load("", "", false, true);
  EXPECT_EQ(loader.find("does_not_exist"), nullptr);
}

// ---------------------------------------------------------------------------
// Manifest filtering
// ---------------------------------------------------------------------------

TEST_F(LoaderTest, ManifestDisabledExcludesTool) {
  GTEST_SKIP() << "TODO: implement Loader::apply_manifest";
  write_manifest(root_ / "tools",
                 R"({"tools":[{"name":"read_file","enabled":false}]})");
  ur::Loader loader(paths_);
  loader.load("", "", false, true);
  EXPECT_EQ(loader.find("read_file"), nullptr);
}

TEST_F(LoaderTest, MissingManifestUsesDefaults) {
  GTEST_SKIP() << "TODO: implement Loader::apply_manifest";
  // No tools.json — all built-ins should be registered with default settings.
  ur::Loader loader(paths_);
  loader.load("", "", false, true);
  EXPECT_NE(loader.find("read_file"), nullptr);
}

// ---------------------------------------------------------------------------
// CLI flag filtering
// ---------------------------------------------------------------------------

TEST_F(LoaderTest, NoToolsProducesEmptyActiveList) {
  GTEST_SKIP() << "TODO: implement Loader::apply_filters";
  ur::Loader loader(paths_);
  loader.load("", "", /*no_tools=*/true, true);
  EXPECT_TRUE(loader.active_tools().empty());
}

TEST_F(LoaderTest, RequiresAllowAllToolExcludedWithoutFlag) {
  GTEST_SKIP() << "TODO: implement Loader::apply_filters";
  // bash requires --allow-all; without it, it must not appear.
  ur::Loader loader(paths_);
  loader.load("", "", false, /*allow_all=*/false);
  EXPECT_EQ(loader.find("bash"), nullptr);
}

TEST_F(LoaderTest, RequiresAllowAllToolIncludedWithFlag) {
  GTEST_SKIP() << "TODO: implement Loader::apply_filters";
  ur::Loader loader(paths_);
  loader.load("", "", false, /*allow_all=*/true);
  EXPECT_NE(loader.find("bash"), nullptr);
}

TEST_F(LoaderTest, AllowListFiltersToNamedTools) {
  GTEST_SKIP() << "TODO: implement Loader::apply_filters";
  ur::Loader loader(paths_);
  loader.load("read_file", "", false, true);
  EXPECT_NE(loader.find("read_file"), nullptr);
  EXPECT_EQ(loader.find("write_file"), nullptr);
}

TEST_F(LoaderTest, DenyListRemovesNamedTool) {
  GTEST_SKIP() << "TODO: implement Loader::apply_filters";
  ur::Loader loader(paths_);
  loader.load("", "write_file", false, true);
  EXPECT_EQ(loader.find("write_file"), nullptr);
  EXPECT_NE(loader.find("read_file"), nullptr);
}
