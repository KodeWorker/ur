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
      } else if (flag == "--persona") {
        ctx.db.drop_persona();
        std::cout << "Persona cleared.\n";
      } else {
        ctx.logger.error(
            "Unknown flag: " + flag +
            "\nUsage: ur clean [--workspace|--database|--persona]");
        return 1;
      }
    } else {
      ctx.logger.error(
          "Too many arguments for clean command.\nUsage: ur clean "
          "[--workspace|--database|--persona]");
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
          "[--model=<name>] [--system-prompt=<text>|@<path>] "
          "[--allow=<tool,...>] [--deny=<tool,...>] [--no-tools] "
          "[--allow-all]");
      return 1;
    }

    std::string prompt(argv[2]);
    std::string system_prompt;
    const char* env_model = std::getenv("UR_LLM_MODEL");
    std::string model = env_model ? env_model : "";
    bool allow_all = false;
    bool no_tools = false;
    std::string allow_list;
    std::string deny_list;

    for (int i = 3; i < argc; ++i) {
      std::string arg(argv[i]);
      if (arg.rfind("--model=", 0) == 0) {
        model = arg.substr(8);
      } else if (arg.rfind("--system-prompt=", 0) == 0) {
        std::string val = arg.substr(16);
        if (!val.empty() && val[0] == '@') {
          // @<path> — read from file.
          std::string path = val.substr(1);
          if (path.empty()) {
            ctx.logger.error("--system-prompt=@ requires a file path");
            return 1;
          }
          std::ifstream f(path);
          if (!f) {
            ctx.logger.error("cannot open system prompt file: " + path);
            return 1;
          }
          system_prompt = std::string(std::istreambuf_iterator<char>(f),
                                      std::istreambuf_iterator<char>());
        } else {
          // Inline string — use directly.
          system_prompt = val;
        }
      } else if (arg.rfind("--allow=", 0) == 0) {
        // Phase 4: pass whitelist to tool loader.
        allow_list = arg.substr(8);
      } else if (arg.rfind("--deny=", 0) == 0) {
        // Phase 4: pass blacklist to tool loader.
        deny_list = arg.substr(7);
      } else if (arg == "--no-tools") {
        // Phase 4: disable tool loading entirely.
        no_tools = true;
      } else if (arg == "--allow-all") {
        // Phase 4: bypass sandbox path enforcement.
        allow_all = true;
      } else {
        ctx.logger.error("Unknown argument: " + arg +
                         "\nUsage: ur run <prompt> [--model=<name>] "
                         "[--system-prompt=<text>|@<path>] "
                         "[--allow=<tool,...>] [--deny=<tool,...>] "
                         "[--no-tools] [--allow-all]");
        return 1;
      }
    }

    if (!allow_list.empty() && !deny_list.empty()) {
      ctx.logger.error("--allow and --deny are mutually exclusive");
      return 1;
    }

    // Phase 4: wire allow_all, no_tools, allow_list, deny_list into tool
    // loader.
    (void)allow_all;
    (void)no_tools;
    (void)allow_list;
    (void)deny_list;
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
      } else {
        ctx.logger.error("Unknown argument: " + arg +
                         "\nUsage: ur chat [--continue=<id|prefix|title>]"
                         " [--model=<name>]");
        return 1;
      }
    }
    ctx.db.init_schema();
    HttpProvider provider = make_http_provider();
    Chat chat(ctx.db, ctx.logger);
    std::unique_ptr<Tui> tui = make_tui();
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
