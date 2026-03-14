#pragma once

#include <string>

#include "tools/tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// WebFetchTool — fetch the text content of a URL.
//
// Input schema:  {"url": "<string>"}
// Sandbox:       none — external HTTP call
// requires_allow_all: true
//
// Returns the response body as plain text (HTML stripped if possible).
// On HTTP error returns is_error = true with the status code.
// ---------------------------------------------------------------------------
class WebFetchTool : public Tool {
 public:
  std::string name() const override { return "web_fetch"; }
  std::string description() const override;
  std::string input_schema_json() const override;
  bool requires_allow_all() const override { return true; }
  ToolResult execute(const std::string& args_json,
                     const SandboxPolicy& policy) override;
};

}  // namespace ur
