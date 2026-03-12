#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ur {

// Callback for streaming token delivery.
// Called once per chunk as it arrives from the server.
using TokenCallback = std::function<void(const std::string&)>;

// A single message in a conversation turn.
// role is passed through to the server as-is.
// Common values: "system", "user", "assistant".
// Phase 4+ tool calling adds: "tool" (tool result), and an "assistant" message
// with a tool_calls field — extend this struct when that phase is reached.
struct Message {
  std::string role;
  std::string content;
};

// Result returned by Provider::complete().
struct CompletionResult {
  std::string content;  // assistant reply (cleaned, no <think> block)
  std::string
      reasoning_content;      // from reasoning_content field if present,
                              // or extracted from <think>…</think> in content
  int prompt_tokens = 0;      // usage.prompt_tokens from API; 0 if absent
  int completion_tokens = 0;  // usage.completion_tokens from API; 0 if absent
};

// Server capabilities fetched once at startup.
struct ServerInfo {
  int context_length = 0;  // model's maximum context window in tokens;
                           // 0 if the server does not report it
};

// Abstract interface for an LLM provider.
class Provider {
 public:
  virtual ~Provider() = default;

  // Fetch server capabilities (context window size, etc.).
  // Called once at startup; result may be cached by the caller.
  // Returns a zeroed ServerInfo if the server does not expose this data.
  // Does not throw — failures are silent (fields stay 0).
  virtual ServerInfo server_info() = 0;

  // Send messages to the model and return the completion result.
  // model: passed through to the server as-is; may be empty.
  // Throws std::runtime_error on failure.
  virtual CompletionResult complete(const std::vector<Message>& messages,
                                    const std::string& model) = 0;

  // Stream a completion, delivering tokens incrementally via callbacks.
  // token_cb:     called for each content chunk (may be nullptr).
  // reasoning_cb: called for each reasoning chunk, e.g. reasoning_content
  //               field or <think> block content (may be nullptr).
  // Throws std::runtime_error on failure.
  virtual void stream(const std::vector<Message>& messages,
                      const std::string& model, const TokenCallback& token_cb,
                      const TokenCallback& reasoning_cb) = 0;
};

}  // namespace ur
