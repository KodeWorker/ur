#pragma once

#include <string>

#include "tools/tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// BashTool — execute a shell command and capture its output.
//
// Input schema:  {"command": "<string>"}
// Sandbox:       none — no path restriction; full shell access
// requires_allow_all: true — only available when --allow-all is set
//
// Timeout: configurable via tools.json "timeout" (seconds); default 10s.
// stdout + stderr are both captured and returned as the tool result.
// Non-zero exit code is reported as is_error = true.
// ---------------------------------------------------------------------------
class BashTool : public Tool {
 public:
  explicit BashTool(int timeout_seconds = 10);

  std::string name() const override { return "bash"; }
  std::string description() const override;
  std::string input_schema_json() const override;
  bool requires_allow_all() const override { return true; }
  ToolResult execute(const std::string& args_json,
                     const SandboxPolicy& policy) override;

 private:
  int timeout_seconds_;
};

}  // namespace ur
