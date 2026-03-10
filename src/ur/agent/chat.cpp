#include "chat.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

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
  // TODO: scan raw for a leading <think> tag (skip whitespace first);
  //       find the matching </think>; copy the content between them into
  //       reasoning_out; return everything after </think> with leading
  //       whitespace trimmed. If no <think> found, set reasoning_out = {}
  //       and return raw unchanged.
  reasoning_out = {};
  return raw;
}

// static
std::vector<Message> Chat::build_window(const std::vector<Message>& history,
                                        const std::string& system_prompt,
                                        size_t window) {
  // TODO: iterate history; keep only messages with role "user" or "assistant";
  //       take the last `window` of those; prepend Message{"system",
  //       system_prompt} if system_prompt is non-empty; return the result.
  (void)history;
  (void)system_prompt;
  (void)window;
  return {};
}

void Chat::run(const ChatOptions& opts, Provider& provider, Tui& tui) {
  // TODO:
  // 1. Load or create session:
  //      If opts.continue_id non-empty: verify db_.session_exists(); load
  //      prior messages via db_.select_messages() into in-memory history.
  //      Otherwise: generate session_id = generate_id(); call
  //      db_.insert_session(session_id, title="", opts.model, now, now).
  //
  // 2. Event loop — driven by tui.read_input():
  //    a. input = tui.read_input() — blocks in ftxui event loop.
  //    b. If input starts with '/':
  //         "/exit" → break out of loop.
  //         Unknown → tui.print_response("[unknown command: " + input + "]").
  //         continue.
  //    c. Persist user turn:
  //         db_.insert_message(generate_id(), session_id, "user", input, now).
  //         history.push_back({"user", input}).
  //    d. Inference:
  //         tui.start_spinner().
  //         raw = provider.complete(build_window(history, system_prompt,
  //                                              opts.context_window),
  //                                 opts.model).
  //         tui.stop_spinner().
  //    e. Reasoning extraction:
  //         std::string reasoning;
  //         content = strip_think(raw, reasoning).
  //         If reasoning non-empty:
  //           db_.insert_message(generate_id(), session_id, "reason",
  //                              reasoning, now).
  //           tui.print_reasoning(reasoning).
  //    f. Persist assistant turn:
  //         db_.insert_message(generate_id(), session_id, "assistant",
  //                            content, now).
  //         db_.touch_session(session_id, now).
  //         history.push_back({"assistant", content}).
  //    g. Render + persona update:
  //         tui.print_response(content).
  //         persona_updater.maybe_update(history, input, content).
  (void)opts;
  (void)provider;
  (void)tui;
  (void)generate_id;  // suppress unused warning until implemented
}

}  // namespace ur
