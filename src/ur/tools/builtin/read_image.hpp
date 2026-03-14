#pragma once

#include <string>

#include "tools/tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// ReadImageTool — validate and pass an image path to a vision-capable model.
//
// Input schema:  {"path": "<string>"}
// Sandbox:       workspace-constrained — calls Sandbox::validate(path, policy)
// requires_allow_all: true
//
// This tool does NOT encode the image itself.  It validates the path exists
// and is inside the workspace, then returns the absolute path as a string.
// The LLM server (llama.cpp) handles the actual image loading and encoding.
//
// Disabled by default.  Enabled only when:
//   - tools.json: "enabled": true
//   - ServerInfo::supports_vision == true  (checked in
//   Loader::register_builtins)
// ---------------------------------------------------------------------------
class ReadImageTool : public Tool {
 public:
  std::string name() const override { return "read_image"; }
  std::string description() const override;
  std::string input_schema_json() const override;
  bool requires_allow_all() const override { return true; }
  ToolResult execute(const std::string& args_json,
                     const SandboxPolicy& policy) override;
};

}  // namespace ur
