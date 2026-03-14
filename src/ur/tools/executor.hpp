#pragma once

#include <functional>
#include <string>
#include <vector>

#include "log/logger.hpp"
#include "memory/database.hpp"
#include "tool.hpp"

namespace ur {

class Loader;

// ---------------------------------------------------------------------------
// Executor — routes ToolCalls from the LLM to the correct Tool instance,
// enforces sandbox policy, manages human audit, and persists audit records.
//
// One Executor is constructed per agent run (Runner or Chat) and reused
// across all turns in that run.
//
// Separation of concerns:
//   - Executor does NOT know about Tui — audit approval is injected via a
//     callback (audit_cb), so Chat and Runner can provide different UIs.
//   - Executor does NOT parse args_json for paths — each Tool::execute() is
//     responsible for calling Sandbox::validate() on its own path arguments.
// ---------------------------------------------------------------------------
class Executor {
 public:
  // loader:  provides Tool* lookup and active tool list.
  // policy:  sandbox constraints — constructed once from CLI flags and reused.
  // db:      for persisting tool_audit messages.
  // logger:  for logging execution events and errors.
  Executor(Loader& loader, const SandboxPolicy& policy, Database& db,
           Logger& logger);

  // Execute all tool calls from a single LLM response turn.
  //
  // calls:    tool calls parsed from the assistant message's tool_calls array.
  // session_id: used when persisting tool_audit messages to the database.
  // audit_cb: called once per call when human approval is required
  //           (i.e. !policy.allow_all).  Return true = approved, false =
  //           rejected. The lambda is provided by the caller:
  //             Chat  → tui.request_tool_audit(name, args_json)
  //             Runner → stderr prompt + stdin y/n (auto-reject if !isatty)
  //
  // Returns one ToolResult per input ToolCall, in the same order.
  // Never throws — errors are returned as ToolResult{msg, true}.
  std::vector<ToolResult> execute_all(
      const std::vector<ToolCall>& calls, const std::string& session_id,
      std::function<bool(const ToolCall&)> audit_cb);

 private:
  // Execute a single call after audit has been approved (or bypassed).
  // Calls tool->execute() and returns the result.
  ToolResult execute_one(Tool* tool, const ToolCall& call);

  // Persist a tool_audit message to the session.
  // content format: "approved: <name> | <args>" or "rejected: <name> | <args>"
  void persist_audit(const std::string& session_id, const std::string& content);

  Loader& loader_;
  const SandboxPolicy& policy_;
  Database& db_;
  Logger& logger_;
};

}  // namespace ur
