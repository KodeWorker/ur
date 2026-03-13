#include "chat.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "agent/persona_updater.hpp"
#include "cli/tui.hpp"
#include "memory/crypto.hpp"

namespace ur {

// Generate a 32-char hex ID from 16 random bytes (same pattern as Runner).
static std::string generate_id() {
  std::string bytes = random_bytes(16);
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (unsigned char c : bytes) {
    oss << std::setw(2) << static_cast<int>(c);
  }
  return oss.str();
}

Chat::Chat(Database& db, Logger& logger) : db_(db), logger_(logger) {}

// static
std::string Chat::strip_think(const std::string& raw,
                              std::string& reasoning_out) {
  reasoning_out = {};

  // Skip leading whitespace to find the first non-space character.
  const size_t start = raw.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return raw;

  const std::string kOpen = "<think>";
  const std::string kClose = "</think>";

  // No leading <think> tag — return unchanged.
  if (raw.compare(start, kOpen.size(), kOpen) != 0) return raw;

  const size_t content_start = start + kOpen.size();
  const size_t close_pos = raw.find(kClose, content_start);

  // Unclosed <think> — treat whole string as content, no reasoning.
  if (close_pos == std::string::npos) return raw;

  reasoning_out = raw.substr(content_start, close_pos - content_start);

  // Everything after </think> with leading whitespace trimmed.
  const size_t after = close_pos + kClose.size();
  const size_t content_pos = raw.find_first_not_of(" \t\n\r", after);
  if (content_pos == std::string::npos) return {};
  return raw.substr(content_pos);
}

// static
std::vector<Message> Chat::build_window(const std::vector<Message>& history,
                                        const std::string& system_prompt,
                                        size_t window) {
  // Collect only user/assistant messages — "reason" is never sent to the model.
  std::vector<Message> filtered;
  filtered.reserve(history.size());
  std::copy_if(history.begin(), history.end(), std::back_inserter(filtered),
               [](const Message& m) {
                 return m.role == "user" || m.role == "assistant";
               });

  // Cap to the last `window` messages.
  if (filtered.size() > window) {
    filtered.erase(
        filtered.begin(),
        filtered.begin() + static_cast<ptrdiff_t>(filtered.size() - window));
  }

  // Prepend system message if provided.
  if (!system_prompt.empty()) {
    filtered.insert(filtered.begin(), {"system", system_prompt});
  }

  return filtered;
}

void Chat::run(const ChatOptions& opts, Provider& provider, Tui& tui) {
  // Fetch server context length once for the status footer.
  const ServerInfo srv = provider.server_info();

  // ── 1. Load or create session ─────────────────────────────────────────────

  std::string session_id;
  std::vector<Message> history;  // in-memory context (all roles)

  const int64_t session_start = static_cast<int64_t>(std::time(nullptr));

  if (!opts.continue_id.empty()) {
    if (!db_.session_exists(opts.continue_id)) {
      throw std::runtime_error("session not found: " + opts.continue_id);
    }
    session_id = opts.continue_id;

    // Replay prior messages into TUI and in-memory history.
    for (const auto& row : db_.select_messages(session_id)) {
      history.push_back({row.role, row.content});
      if (row.role == "user")
        tui.print_user(row.content);
      else if (row.role == "reason")
        tui.print_reasoning(row.content);
      else if (row.role == "assistant")
        tui.print_response(row.content);
    }
    logger_.info("resumed session " + session_id);
  } else {
    session_id = generate_id();
    db_.insert_session(session_id, /*title=*/"", opts.model, session_start,
                       session_start);
    logger_.info("new session " + session_id);
  }

  PersonaUpdater persona(db_, provider, logger_, opts.model);

  // ── 2. Event loop ─────────────────────────────────────────────────────────

  while (true) {
    std::string input = tui.read_input();

    // Trim trailing whitespace (spaces, \r, \n) from input field.
    while (!input.empty() && (input.back() == ' ' || input.back() == '\r' ||
                              input.back() == '\n' || input.back() == '\t'))
      input.pop_back();

    // Empty string signals Ctrl-C or TUI exit.
    if (input.empty()) break;

    // ── Slash commands ───────────────────────────────────────────────────────
    if (input[0] == '/') {
      tui.print_user(input);

      if (input == "/help") {
        tui.print_response(
            "ℹ️available commands:\n"
            "/help                - show this help message\n"
            "/exit                - exit the chat session\n"
            "/compact             - summarise and compress context\n"
            "/clear               - clear in-memory context\n"
            "/title <text>        - set session title\n"
            "/persona             - generate persona\n"
            "/save-prompt <file>  - save system prompt to file\n"
            "/load-prompt <file>  - load system prompt from file");
        continue;
      } else if (input == "/exit") {
        tui.print_response("👋goodbye");
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        break;
      } else if (input == "/compact") {
        // Stub: full summarisation deferred to Phase 5.
        tui.print_error("/compact is not yet implemented");
      } else if (input == "/clear") {
        history.clear();  // only clears in-memory history; prior messages
                          // remain in DB and session context
        tui.print_response("🧹context cleared");
      } else if (input == "/persona") {
        persona.maybe_update(history, /*user_msg=*/"", /*assistant_msg=*/"",
                             /*force_update=*/true);
        std::string out;
        for (const auto& p : db_.select_persona())
          out += p.key + ": " + p.value + "\n";
        tui.print_response(out.empty() ? "🪞no persona facts yet"
                                       : "🪞persona updated");
      } else if (input.rfind("/save-prompt ", 0) == 0) {
        const std::string path = input.substr(13);
        if (path.empty()) {
          tui.print_error("usage: /save-prompt <file>");
        } else {
          try {
            std::ofstream f(path);
            if (!f) throw std::runtime_error("cannot open: " + path);
            f << tui.system_prompt();
            tui.print_response("💾system prompt saved to: " + path);
          } catch (const std::exception& e) {
            tui.print_error(std::string("failed to save system prompt: ") +
                            e.what());
          }
        }
      } else if (input.rfind("/load-prompt ", 0) == 0) {
        const std::string path = input.substr(13);
        if (path.empty()) {
          tui.print_error("usage: /load-prompt <file>");
        } else {
          try {
            std::ifstream f(path);
            if (!f) throw std::runtime_error("cannot open: " + path);
            std::string new_prompt((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
            tui.set_system_prompt(new_prompt);
            tui.print_response("📁system prompt loaded from: " + path);
          } catch (const std::exception& e) {
            tui.print_error(std::string("failed to load system prompt: ") +
                            e.what());
          }
        }
      } else if (input.rfind("/title ", 0) == 0) {
        const std::string new_title = input.substr(7);
        if (new_title.empty()) {
          tui.print_error("usage: /title <text>");
        } else {
          try {
            db_.update_session_title(session_id, new_title);
            tui.print_response("🏷️title set: " + new_title);
          } catch (const std::exception& e) {
            tui.print_error(std::string("failed to set title: ") + e.what());
          }
        }
      } else if (input == "/title" || input == "/save-prompt" ||
                 input == "/load-prompt") {
        tui.print_error("usage: " + input + " <text>");
      } else {
        tui.print_error("unknown command: " + input);
      }
      continue;
    }

    const int64_t now = static_cast<int64_t>(std::time(nullptr));

    // ── c. Echo and persist user turn ────────────────────────────────────────
    tui.print_user(input);
    db_.insert_message(generate_id(), session_id, "user", input, now);
    history.push_back({"user", input});

    // ── d. Inference (streaming)
    // ──────────────────────────────────────────────
    tui.start_spinner();
    std::string reasoning;
    std::string content;
    bool spinner_stopped = false;
    try {
      provider.stream(
          build_window(history, tui.system_prompt(), opts.context_window),
          opts.model,
          // token_cb: stop spinner on first chunk, stream to TUI.
          [&](const std::string& chunk) {
            if (!spinner_stopped) {
              tui.stop_spinner();
              spinner_stopped = true;
            }
            content += chunk;
            tui.print_response_chunk(chunk);
          },
          // reasoning_cb: stream to TUI in real-time, accumulate for DB.
          [&](const std::string& chunk) {
            if (!spinner_stopped) {
              tui.stop_spinner();
              spinner_stopped = true;
            }
            reasoning += chunk;
            tui.print_reasoning_chunk(chunk);
          });
    } catch (const std::exception& e) {
      if (!spinner_stopped) tui.stop_spinner();
      tui.print_error(e.what());
      logger_.error(std::string("provider error: ") + e.what());
      continue;
    }
    if (!spinner_stopped) tui.stop_spinner();

    // ── e. <think> fallback for models that embed reasoning in content
    // ──────── reasoning_content delta chunks are already streamed via
    // reasoning_cb. For models that embed <think>…</think> inline in content
    // (no separate reasoning_content field), strip and display now as a full
    // block.
    if (reasoning.empty()) {
      content = strip_think(content, reasoning);
      if (!reasoning.empty()) tui.print_reasoning(reasoning);
    }

    if (!reasoning.empty()) {
      db_.insert_message(generate_id(), session_id, "reason", reasoning, now);
    }

    // ── f. Persist assistant turn
    // ─────────────────────────────────────────────
    db_.insert_message(generate_id(), session_id, "assistant", content, now);
    db_.touch_session(session_id, now);
    history.push_back({"assistant", content});

    // ── g. Status + persona
    // ─────────────────────────────────────────────────── prompt_tokens is not
    // available from stream(); set to 0 for now.
    // TODO(phase5): parse the final usage chunk to get token counts.
    tui.set_status(0, srv.context_length, "/compact to summarize");

    persona.maybe_update(history, input, content);
  }

  logger_.info("session ended: " + session_id);
}

}  // namespace ur
