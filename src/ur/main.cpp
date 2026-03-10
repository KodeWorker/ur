#include <iostream>
#include <string>

#include "cli/command.hpp"
#include "cli/context.hpp"
#include "env.hpp"

static void print_usage() {
  std::cerr << "usage: ur <command> [options]\n"
            << "\n"
            << "commands:\n"
            << "  init                       create workspace and initialise "
               "database\n"
            << "  clean [--database|--workspace]  remove workspace artifacts\n"
            << "  run <prompt> [--model=<name>] [--system-prompt=<file>]\n"
            << "                             one-shot LLM request\n";
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
  ur::Context ctx = ur::make_context();

  if (cmd == "init") return ur::cmd_init(ctx, argc, argv);
  if (cmd == "clean") return ur::cmd_clean(ctx, argc, argv);
  if (cmd == "run") return ur::cmd_run(ctx, argc, argv);

  std::cerr << "ur: unknown command '" << cmd << "'\n";
  print_usage();
  return 1;
}
