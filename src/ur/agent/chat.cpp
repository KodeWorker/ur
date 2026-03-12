#include "chat.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
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

  PersonaUpdater persona(db_, provider, opts.model);

  // ── 2. Event loop ─────────────────────────────────────────────────────────

  while (true) {
    const std::string input = tui.read_input();

    // Empty string signals Ctrl-C or TUI exit.
    if (input.empty()) break;

    // ── Slash commands ───────────────────────────────────────────────────────
    if (input[0] == '/') {
      if (input == "/exit") {
        break;
      } else if (input == "/compact") {
        // Stub: full summarisation deferred to Phase 5.
        tui.print_error("/compact is not yet implemented");
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

    // ── d. Inference ─────────────────────────────────────────────────────────
    tui.start_spinner();
    CompletionResult cr;
    try {
      cr = provider.complete(
          build_window(history, tui.system_prompt(), opts.context_window),
          opts.model);
    } catch (const std::exception& e) {
      tui.stop_spinner();
      tui.print_error(e.what());
      logger_.error(std::string("provider error: ") + e.what());
      continue;
    }
    tui.stop_spinner();

    // ── e. Reasoning extraction
    // ─────────────────────────────────────────────── Prefer dedicated
    // reasoning_content field; fall back to <think> stripping for models that
    // embed reasoning inline (DeepSeek-R1, QwQ).
    std::string reasoning;
    std::string content;
    if (!cr.reasoning_content.empty()) {
      reasoning = cr.reasoning_content;
      content = cr.content;
    } else {
      content = strip_think(cr.content, reasoning);
    }

    if (!reasoning.empty()) {
      db_.insert_message(generate_id(), session_id, "reason", reasoning, now);
      tui.print_reasoning(reasoning);
    }

    // ── f. Persist assistant turn
    // ─────────────────────────────────────────────
    db_.insert_message(generate_id(), session_id, "assistant", content, now);
    db_.touch_session(session_id, now);
    history.push_back({"assistant", content});

    // ── g. Render + status + persona ─────────────────────────────────────────
    tui.print_response(content);
    tui.set_status(cr.prompt_tokens, srv.context_length,
                   "/compact to summarize");

    persona.maybe_update(history, input, content);
  }

  logger_.info("session ended: " + session_id);
}

}  // namespace ur
