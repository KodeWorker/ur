#pragma once

#include <string>

#include "tools/tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// WriteFileTool — create or overwrite a file inside $root/workspace/.
//
// Input schema:  {"path": "<string>", "content": "<string>"}
// Sandbox:       workspace-constrained — calls Sandbox::validate(path, policy)
// requires_allow_all: false
// ---------------------------------------------------------------------------
class WriteFileTool : public Tool {
 public:
  std::string name() const override { return "write_file"; }
  std::string description() const override;
  std::string input_schema_json() const override;
  bool requires_allow_all() const override { return false; }
  ToolResult execute(const std::string& args_json,
                     const SandboxPolicy& policy) override;
};

}  // namespace ur
