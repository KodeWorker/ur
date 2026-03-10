#include "command.hpp"

#include <iostream>
#include <string>

#include "agent/runner.hpp"
#include "llm/http_provider.hpp"

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
        ctx.logger.error("Unknown flag: " + flag +
                         "\nUsage: ur clean [--workspace|--database]");
        return 1;
      }
    } else {
      ctx.logger.error(
          "Too many arguments for clean command.\nUsage: ur clean "
          "[--workspace|--database]");
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

int cmd_run(Context& ctx, int argc, char** argv) {
  // TODO:
  // 1. Parse arguments:
  //      prompt           = argv[2]  (required; error if missing)
  //      model            = value of --model=<name> if present,
  //                         else std::getenv("UR_LLM_MODEL"), else ""
  //      system_prompt    = contents of file at --system-prompt=<path>
  //                         if present; error if file cannot be read
  //      allow_all        = true if --allow-all flag is present (Phase 4)
  //
  // 2. Construct provider:
  //      HttpProvider provider = make_http_provider();
  //
  // 3. Construct runner:
  //      Runner runner(ctx.db, ctx.logger);
  //
  // 4. Call runner.run(prompt, system_prompt, model, provider).
  //    Print result.response to stdout.
  //    Optionally log session_id at info level.
  //
  // 5. Return 0 on success, 1 on any exception (log via ctx.logger.error).
  (void)ctx;
  (void)argc;
  (void)argv;
  std::cerr << "cmd_run: not implemented\n";
  return 1;
}

}  // namespace ur
