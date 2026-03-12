#include "command.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "agent/chat.hpp"
#include "agent/runner.hpp"
#include "cli/tui.hpp"
#include "llm/http_provider.hpp"
#include "memory/crypto.hpp"

namespace ur {

int cmd_init(Context& ctx, int /*argc*/, char** /*argv*/) {
  try {
    init_workspace(ctx.paths);
    ctx.db.init_schema();
    const std::filesystem::path key_path = ctx.paths.key / "secret.key";
    bool key_existed = std::filesystem::exists(key_path);
    generate_key(key_path);
    std::cout << "Workspace initialized at " << ctx.paths.root << "\n";
    if (!key_existed) {
      std::cout << "Encryption key generated.\n";
    }
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
  try {
    if (argc < 3) {
      ctx.logger.error(
          "Missing prompt argument.\nUsage: ur run <prompt> "
          "[--model=<name>] [--system-prompt=<path>] [--allow-all]");
      return 1;
    }
    std::string prompt(argv[2]);
    std::string system_prompt;
    const char* env_model = std::getenv("UR_LLM_MODEL");
    std::string model = env_model ? env_model : "";
    // Parse optional flags. For simplicity, we require --model and
    // --system-prompt to come after the prompt and allow them in any order.
    // Phase 4 will have more robust parsing.
    for (int i = 3; i < argc; ++i) {
      std::string arg(argv[i]);
      if (arg.rfind("--model=", 0) == 0) {
        model = arg.substr(8);
      } else if (arg.rfind("--system-prompt=", 0) == 0) {
        std::string path = arg.substr(16);
        try {
          // Read the system prompt from the specified file path.
          std::ifstream f(path);
          if (!f) throw std::runtime_error("cannot open: " + path);
          system_prompt = std::string(std::istreambuf_iterator<char>(f),
                                      std::istreambuf_iterator<char>());
        } catch (const std::exception& e) {
          ctx.logger.error("Failed to read system prompt file: " +
                           std::string(e.what()));
          return 1;
        }
      } else if (arg == "--allow-all") {
        // TODO: implement allow-all flag in Runner and pass it down to control
        // encryption behavior. For now, just log a warning that messages will
        // be stored unencrypted if the provider doesn't support encryption.
        // allow_all = true; // Phase 4
        ctx.logger.warn(
            "--allow-all flag is not implemented yet. "
            "Messages will be stored according to the provider's "
            "capabilities.");
      } else {
        ctx.logger.error("Unknown argument: " + arg +
                         "\nUsage: ur run <prompt> [--model=<name>] "
                         "[--system-prompt=<path>] [--allow-all]");
        return 1;
      }
    }
    ctx.db.init_schema();
    HttpProvider provider = make_http_provider();
    Runner runner(ctx.db, ctx.logger);
    RunResult result = runner.run(prompt, system_prompt, model, provider);
    std::cout << result.response << "\n";
    ctx.logger.info("Session ID: " + result.session_id);
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

int cmd_chat(Context& ctx, int argc, char** argv) {
  try {
    ChatOptions opts;
    const char* env_model = std::getenv("UR_LLM_MODEL");
    opts.model = env_model ? env_model : "";
    for (int i = 2; i < argc; ++i) {
      std::string arg(argv[i]);
      if (arg.rfind("--model=", 0) == 0) {
        opts.model = arg.substr(8);
      } else if (arg.rfind("--continue=", 0) == 0) {
        opts.continue_id = arg.substr(11);
      } else if (arg.rfind("--system-prompt=", 0) == 0) {
        std::string path = arg.substr(16);
        try {
          std::ifstream f(path);
          if (!f) throw std::runtime_error("cannot open: " + path);
          opts.system_prompt = std::string(std::istreambuf_iterator<char>(f),
                                           std::istreambuf_iterator<char>());
        } catch (const std::exception& e) {
          ctx.logger.error("Failed to read system prompt file: " +
                           std::string(e.what()));
          return 1;
        }
      } else {
        ctx.logger.error("Unknown argument: " + arg +
                         "\nUsage: ur chat [--continue=<id>] [--model=<name>]"
                         " [--system-prompt=<file>]");
        return 1;
      }
    }
    ctx.db.init_schema();
    HttpProvider provider = make_http_provider();
    Chat chat(ctx.db, ctx.logger);
    std::unique_ptr<Tui> tui = make_tui(opts.system_prompt);
    chat.run(opts, provider, *tui);
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

int cmd_history(Context& ctx, int argc, char** argv) {
  try {
    ctx.db.init_schema();
    if (argc == 2) {
      // TODO: ctx.db.select_sessions() → print id, title, created_at, model
      //       for each row.
    } else if (argc == 3) {
      // TODO: ctx.db.select_messages(argv[2]) → print [role] content for each.
      (void)argv;
    } else {
      ctx.logger.error("Usage: ur history [<id>]");
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

int cmd_persona(Context& ctx, int /*argc*/, char** /*argv*/) {
  try {
    ctx.db.init_schema();
    // TODO: ctx.db.select_persona() → print "key:  value" for each row.
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

}  // namespace ur
