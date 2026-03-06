#include "command.hpp"

#include <iostream>
#include <string>

namespace ur {

int cmd_init(Context &ctx, int /*argc*/, char ** /*argv*/) {
  // TODO:
  // 1. init_workspace(ctx.paths)  — create all five subdirectories
  // 2. ctx.db.init_schema()       — lazy-open DB and create tables
  // 3. Print a confirmation message to stdout
  // Return 0 on success, 1 on error.
  (void)ctx;
  std::cerr << "cmd_init: not implemented\n";
  return 1;
}

int cmd_clean(Context &ctx, int argc, char **argv) {
  // Parse the optional flag from argv[2]:
  //   --workspace : remove workspace contents only
  //   --database  : drop all database tables only
  //   (no flag)   : do both
  //
  // TODO:
  //   --workspace → remove_workspace(ctx.paths)
  //   --database  → ctx.db.drop_all()
  //   (no flag)   → both of the above
  // Return 0 on success, 1 on unknown flag or error.
  (void)ctx;
  (void)argc;
  (void)argv;
  std::cerr << "cmd_clean: not implemented\n";
  return 1;
}

} // namespace ur
