#include "web_fetch.hpp"

#include <string>

namespace ur {

std::string WebFetchTool::description() const {
  return "Fetch the text content of a URL. "
         "Returns the response body as plain text. "
         "Only available with --allow-all.";
}

std::string WebFetchTool::input_schema_json() const {
  return R"({"type":"object","properties":{"url":{"type":"string","description":"URL to fetch"}},"required":["url"]})";
}

ToolResult WebFetchTool::execute(const std::string& args_json,
                                 const SandboxPolicy& policy) {
  // TODO: implement URL fetch.
  //
  // Steps:
  //   1. Parse args_json; extract "url" field.
  //      On error: return {"invalid args: ...", true}.
  //
  //   2. if (!policy.allow_all) return {"web_fetch requires --allow-all",
  //   true};
  //
  //   3. Parse url into host + path (split on first "/" after scheme).
  //      Detect HTTPS vs HTTP from scheme prefix.
  //
  //   4. Use cpp-httplib (httplib::Client or httplib::SSLClient) to GET.
  //      On connection error: return {"web_fetch: connection failed", true}.
  //      On non-200 status: return {"web_fetch: HTTP " + status, true}.
  //
  //   5. Return response body.
  //      Optionally strip HTML tags for cleaner LLM input (simple regex or
  //      a lightweight parser — keep it simple for Phase 4).
  //
  //   6. return {body, false};

  (void)args_json;
  (void)policy;
  return {"not implemented", true};
}

}  // namespace ur
