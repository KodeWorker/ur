#include "command.hpp"

#include <iostream>
#include <string>

namespace ur {

int cmd_init(Context& ctx, int /*argc*/, char** /*argv*/) {
  try {
    init_workspace(ctx.paths);
    ctx.db.init_schema();
    std::cout << "Workspace initialized at " << ctx.paths.root << "\n";
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

int cmd_clean(Context& ctx, int argc, char** argv) {
  try {
    if (argc == 2) {
      // No flag, do both
      remove_workspace(ctx.paths);
      ctx.db.drop_all();
      std::cout << "Workspace and database cleared.\n";
    } else if (argc == 3) {
      std::string flag(argv[2]);
      if (flag == "--workspace") {
        remove_workspace(ctx.paths);
        std::cout << "Workspace cleared.\n";
      } else if (flag == "--database") {
        ctx.db.drop_all();
        std::cout << "Database cleared.\n";
      } else {
        ctx.logger.error("Unknown flag: " + flag);
        return 1;
      }
    } else {
      ctx.logger.error("Too many arguments for clean command.");
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

}  // namespace ur
