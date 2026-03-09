#pragma once

#include <string>
#include <vector>

namespace ur {

// A single message in a conversation turn.
// role is passed through to the server as-is.
// Common values: "system", "user", "assistant".
// Phase 4+ tool calling adds: "tool" (tool result), and an "assistant" message
// with a tool_calls field — extend this struct when that phase is reached.
struct Message {
  std::string role;
  std::string content;
};

// Abstract interface for an LLM provider.
// Implementations post a list of messages and return the assistant response.
class Provider {
 public:
  virtual ~Provider() = default;

  // Send messages to the model and return the assistant reply.
  // model: passed through to the server as-is; may be empty.
  // Throws std::runtime_error on failure.
  virtual std::string complete(const std::vector<Message>& messages,
                               const std::string& model) = 0;
};

}  // namespace ur
