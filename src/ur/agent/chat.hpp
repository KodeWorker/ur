#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "llm/provider.hpp"
#include "log/logger.hpp"
#include "memory/database.hpp"

namespace ur {

// Forward declaration — full definition in cli/tui.hpp.
class Tui;

struct ChatOptions {
  std::string model;  // passed to provider; empty = server default
  std::string
      system_prompt;        // injected as first message each turn; empty = none
  std::string continue_id;  // session ID to resume; empty = new session
  size_t context_window =
      20;  // max non-system messages in rolling context window
};

class Chat {
 public:
  Chat(Database& db, Logger& logger);

  // Enter the interactive loop. Blocks until the user exits (/exit or Ctrl-C).
  // Requires db.init_schema() to have been called before this.
  void run(const ChatOptions& opts, Provider& provider, Tui& tui);

  // Strip <think>...</think> prefix from raw provider output.
  // reasoning_out receives the extracted block content (empty if none present).
  // Returns the cleaned content string.
  // Exposed as public static for unit testing.
  static std::string strip_think(const std::string& raw,
                                 std::string& reasoning_out);

  // Build the context window to send to the provider.
  // Prepends a "system" message if system_prompt is non-empty.
  // Excludes "reason" role messages — never sent to the model.
  // Caps to window most recent "user"/"assistant" messages.
  // Exposed as public static for unit testing.
  static std::vector<Message> build_window(const std::vector<Message>& history,
                                           const std::string& system_prompt,
                                           size_t window);

 private:
  Database& db_;
  Logger& logger_;
};

}  // namespace ur
