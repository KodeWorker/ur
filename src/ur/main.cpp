#include <iostream>
#include <string>

#include "cli/command.hpp"
#include "cli/context.hpp"
#include "env.hpp"
#include "memory/workspace.hpp"

static void print_usage() {
  std::cerr << "usage: ur <command> [options]\n"
            << "\n"
            << "commands:\n"
            << "  init                            create workspace and "
               "initialise database\n"
            << "  clean [--database|--workspace]  remove workspace artifacts\n"
            << "  run <prompt> [--model=<name>] [--system-prompt=<file>]\n"
            << "                                  one-shot LLM request\n"
            << "  chat [--continue=<id>] [--model=<name>]\n"
            << "                                  interactive TUI session\n"
            << "  history [<id>]                  list sessions or show "
               "messages\n"
            << "  persona                         show user persona\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  const std::string cmd = argv[1];

  if (cmd == "--help" || cmd == "-h") {
    print_usage();
    return 0;
  }

  ur::load_dotenv();

  // init must run before make_context() — the key doesn't exist yet.
  if (cmd == "init") {
    try {
      return ur::cmd_init(ur::resolve_paths(), argc, argv);
    } catch (const std::exception& e) {
      std::cerr << "[ERROR] " << e.what() << '\n';
      return 1;
    }
  }

  ur::Context ctx = [&]() -> ur::Context {
    try {
      return ur::make_context();
    } catch (const std::exception& e) {
      std::cerr << "[ERROR] " << e.what() << '\n';
      std::exit(1);
    }
  }();
  std::filesystem::current_path(ctx.paths.workspace);

  if (cmd == "clean") return ur::cmd_clean(ctx, argc, argv);
  if (cmd == "run") return ur::cmd_run(ctx, argc, argv);
  if (cmd == "chat") return ur::cmd_chat(ctx, argc, argv);
  if (cmd == "history") return ur::cmd_history(ctx, argc, argv);
  if (cmd == "persona") return ur::cmd_persona(ctx, argc, argv);

  std::cerr << "ur: unknown command '" << cmd << "'\n";
  print_usage();
  return 1;
}
