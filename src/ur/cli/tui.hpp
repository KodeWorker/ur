#pragma once

#include <memory>
#include <string>

namespace ur {

// Abstract TUI interface — FtxuiTui for production, MockTui in tests.
class Tui {
 public:
  virtual ~Tui() = default;

  // Block until the user submits a line. Returns "" on /exit or Ctrl-C.
  virtual std::string read_input() = 0;

  // Append the assistant response to the chat view.
  virtual void print_response(const std::string& content) = 0;

  // Append a collapsible reasoning block above the response.
  // No-op if reasoning is empty.
  virtual void print_reasoning(const std::string& reasoning) = 0;

  // Show/hide a spinner while waiting for the provider.
  virtual void start_spinner() = 0;
  virtual void stop_spinner() = 0;
};

// Concrete ftxui implementation. Owns a ScreenInteractive event loop.
class FtxuiTui : public Tui {
 public:
  FtxuiTui();
  ~FtxuiTui() override;

  // Non-copyable, non-movable (owns terminal state).
  FtxuiTui(const FtxuiTui&) = delete;
  FtxuiTui& operator=(const FtxuiTui&) = delete;

  std::string read_input() override;
  void print_response(const std::string& content) override;
  void print_reasoning(const std::string& reasoning) override;
  void start_spinner() override;
  void stop_spinner() override;
};

// Construct the production TUI.
std::unique_ptr<Tui> make_tui();

}  // namespace ur
