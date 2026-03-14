#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ur {

// ---------------------------------------------------------------------------
// ToolCall — a single tool invocation requested by the LLM.
// Parsed from the "tool_calls" array in an assistant message.
// ---------------------------------------------------------------------------
struct ToolCall {
  std::string id;         // tool_call_id from LLM response — echoed in result
  std::string name;       // tool name, must match a registered Tool::name()
  std::string args_json;  // JSON object string, schema defined by the tool
};

// ---------------------------------------------------------------------------
// ToolResult — the outcome of executing one ToolCall.
// Returned to the LLM as a "tool" role message.
// ---------------------------------------------------------------------------
struct ToolResult {
  std::string content;  // text returned to the LLM (output, error message, …)
  bool is_error;        // true → LLM is informed the call failed
};

// ---------------------------------------------------------------------------
// SandboxPolicy — execution constraints passed to every Tool::execute() call.
// Constructed once from CLI flags in Executor and handed down unchanged.
// ---------------------------------------------------------------------------
struct SandboxPolicy {
  std::filesystem::path workspace_root;  // absolute path — $root/workspace/
  bool allow_all;  // --allow-all: bypass path enforcement and human audit
};

// ---------------------------------------------------------------------------
// Tool — abstract base for all tools (built-in and plugin).
//
// Built-ins are compiled into ur_lib and registered by Loader before the
// plugin scan.  External plugins (shared libs / scripts) implement the same
// interface and are loaded at runtime.  From Executor's perspective they are
// indistinguishable.
//
// Implementing a new built-in:
//   1. Subclass Tool in src/ur/tools/builtin/<name>.hpp/.cpp
//   2. Override all pure virtuals
//   3. Register in Loader::register_builtins()
// ---------------------------------------------------------------------------
class Tool {
 public:
  virtual ~Tool() = default;

  // Stable identifier used in tools.json, tool_calls, and audit messages.
  virtual std::string name() const = 0;

  // Human-readable description injected into the LLM tool list.
  virtual std::string description() const = 0;

  // JSON Schema object string describing the tool's input parameters.
  // Must conform to the OpenAI function calling schema format, e.g.:
  //   {"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}
  virtual std::string input_schema_json() const = 0;

  // Return true if this tool must only be available under --allow-all.
  // bash, web_search, web_fetch return true.
  // read_file, write_file, read_image return false.
  virtual bool requires_allow_all() const = 0;

  // Execute the tool call.
  // args_json: JSON string matching input_schema_json()
  // policy:    sandbox constraints — check policy.allow_all before bypassing
  //            path enforcement; call Sandbox::validate() for any file paths
  // Must not throw — catch internally and return ToolResult{msg, true}.
  virtual ToolResult execute(const std::string& args_json,
                             const SandboxPolicy& policy) = 0;
};

}  // namespace ur
