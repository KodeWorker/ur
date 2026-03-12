#pragma once

#include <memory>
#include <string>

namespace ur {

// ---------------------------------------------------------------------------
// Tui — abstract interface (FtxuiTui for production, MockTui in tests)
// ---------------------------------------------------------------------------

class Tui {
 public:
  virtual ~Tui() = default;

  // Block until the user submits a line.
  // Returns "" on /exit or Ctrl-C.
  // Lines beginning with '/' are slash commands — returned verbatim so the
  // caller can dispatch them without calling the provider.
  virtual std::string read_input() = 0;

  // Append a user message to the Session tab history view.
  virtual void print_user(const std::string& content) = 0;

  // Append the assistant response to the Session tab history view.
  virtual void print_response(const std::string& content) = 0;

  // Append a collapsible reasoning block above the response.
  // No-op if reasoning is empty.
  virtual void print_reasoning(const std::string& reasoning) = 0;

  // Show an inline error line in the Session tab (e.g. unknown slash command).
  virtual void print_error(const std::string& msg) = 0;

  // Update the one-line status footer in the Session tab.
  // Format: "context: N/ctx_len tokens  |  <hint>"
  // prompt_tokens: usage.prompt_tokens from the last CompletionResult.
  // ctx_len: model's context_length from ServerInfo; 0 = omit the denominator.
  virtual void set_status(int prompt_tokens, int ctx_len,
                          const std::string& hint) = 0;

  // Show/hide a spinner while waiting for the provider.
  virtual void start_spinner() = 0;
  virtual void stop_spinner() = 0;

  // Return the current system prompt text (may have been edited by the user
  // in the System Prompt tab since the session started).
  virtual std::string system_prompt() const = 0;
};

// ---------------------------------------------------------------------------
// FtxuiTui — concrete 4-tab ftxui implementation
//
// Tab 1 — Session:       scrollable history + input box + status footer
// Tab 2 — System Prompt: multi-line editor, mid-session effect, save/load
// Tab 3 — Tools:         checkbox list (placeholder until Phase 4)
// Tab 4 — Options:       ENV-controlled settings, persisted to .env on save
// ---------------------------------------------------------------------------

class FtxuiTui : public Tui {
 public:
  // initial_system_prompt: pre-populates the System Prompt tab editor.
  explicit FtxuiTui(std::string initial_system_prompt = {});
  ~FtxuiTui() override;

  // Non-copyable, non-movable (owns terminal state).
  FtxuiTui(const FtxuiTui&) = delete;
  FtxuiTui& operator=(const FtxuiTui&) = delete;

  std::string read_input() override;
  void print_user(const std::string& content) override;
  void print_response(const std::string& content) override;
  void print_reasoning(const std::string& reasoning) override;
  void print_error(const std::string& msg) override;
  void set_status(int prompt_tokens, int ctx_len,
                  const std::string& hint) override;
  void start_spinner() override;
  void stop_spinner() override;
  std::string system_prompt() const override;
};

// Construct the production TUI, pre-populating the System Prompt tab.
std::unique_ptr<Tui> make_tui(std::string initial_system_prompt = {});

}  // namespace ur
