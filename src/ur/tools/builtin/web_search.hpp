#pragma once

#include <string>

#include "tools/tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// WebSearchTool — query a SearXNG instance and return ranked results.
//
// Input schema:  {"query": "<string>"}
// Sandbox:       none — external HTTP call
// requires_allow_all: true
//
// Backend: HTTP GET to $UR_SEARCH_BASE_URL/search?q=<query>&format=json
// The tool is only registered when UR_SEARCH_BASE_URL is set at load time.
// max_results: configurable via tools.json "max_results"; default 5.
//
// Returns results formatted as:
//   1. <title> — <url>\n<snippet>\n\n2. ...
// ---------------------------------------------------------------------------
class WebSearchTool : public Tool {
 public:
  // base_url: value of UR_SEARCH_BASE_URL (non-empty, caller-validated)
  // max_results: from tools.json manifest; default 5
  explicit WebSearchTool(std::string base_url, int max_results = 5);

  std::string name() const override { return "web_search"; }
  std::string description() const override;
  std::string input_schema_json() const override;
  bool requires_allow_all() const override { return true; }
  ToolResult execute(const std::string& args_json,
                     const SandboxPolicy& policy) override;

 private:
  std::string base_url_;
  int max_results_;
};

}  // namespace ur
