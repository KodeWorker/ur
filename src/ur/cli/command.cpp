#include "command.hpp"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "agent/chat.hpp"
#include "agent/runner.hpp"
#include "cli/tui.hpp"
#include "llm/http_provider.hpp"
#include "memory/crypto.hpp"

namespace ur {

int cmd_init(const Paths& paths, int /*argc*/, char** /*argv*/) {
  try {
    init_workspace(paths);
    const std::filesystem::path key_path = paths.key / "secret.key";
    bool key_existed = std::filesystem::exists(key_path);
    generate_key(key_path);
    const std::string enc_key = load_key(key_path);
    Database(paths.database / "ur.db", enc_key).init_schema();
    std::cout << "Workspace initialized at " << paths.root << "\n";
    if (!key_existed) {
      std::cout << "Encryption key generated.\n";
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] " << e.what() << '\n';
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
    RunResult result = runner.run(
        prompt, system_prompt, model, provider,
        [](const std::string& chunk) { std::cout << chunk << std::flush; },
        nullptr);
    std::cout << '\n';
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
        const std::string val = arg.substr(11);
        // Resolve: exact ID → ID prefix → title.
        ctx.db.init_schema();
        if (ctx.db.session_exists(val)) {
          opts.continue_id = val;
        } else {
          try {
            opts.continue_id = ctx.db.find_session_by_id_prefix(val);
          } catch (const std::exception&) {
            opts.continue_id = ctx.db.find_session_by_title(val);
          }
        }
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
      for (const auto& session : ctx.db.select_sessions()) {
        auto tm = std::localtime(&session.created_at);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        std::cout << session.id << " | " << session.title << " | "
                  << session.model << " | " << oss.str() << '\n';
      }
    } else if (argc == 3) {
      std::string id(argv[2]);
      if (!ctx.db.session_exists(id)) {
        try {
          id = ctx.db.find_session_by_id_prefix(id);
        } catch (const std::exception& e) {
          ctx.logger.error(e.what());
          return 1;
        }
      }
      for (const auto& message : ctx.db.select_messages(id)) {
        std::cout << "[" << message.role << "] " << message.content << '\n';
      }
    } else {
      ctx.logger.error(
          "Too many arguments for history command.\nUsage: ur history "
          "[<session_id>]");
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
    for (const auto& persona : ctx.db.select_persona()) {
      std::cout << persona.key << ": " << persona.value << '\n';
    }
    return 0;
  } catch (const std::exception& e) {
    ctx.logger.error(e.what());
  }
  return 1;
}

}  // namespace ur
