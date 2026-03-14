#include "web_search.hpp"

#include <string>
#include <utility>

namespace ur {

WebSearchTool::WebSearchTool(std::string base_url, int max_results)
    : base_url_(std::move(base_url)), max_results_(max_results) {}

std::string WebSearchTool::description() const {
  return "Search the web using SearXNG and return ranked results. "
         "Only available with --allow-all and UR_SEARCH_BASE_URL set.";
}

std::string WebSearchTool::input_schema_json() const {
  return R"({"type":"object","properties":{"query":{"type":"string","description":"Search query string"}},"required":["query"]})";
}

ToolResult WebSearchTool::execute(const std::string& args_json,
                                  const SandboxPolicy& policy) {
  // TODO: implement SearXNG search.
  //
  // Steps:
  //   1. Parse args_json; extract "query" field.
  //      On error: return {"invalid args: ...", true}.
  //
  //   2. if (!policy.allow_all) return {"web_search requires --allow-all",
  //   true};
  //
  //   3. URL-encode the query string (percent-encode non-safe characters).
  //
  //   4. Build request URL:
  //        base_url_ + "/search?q=" + encoded_query +
  //        "&format=json&pageno=1"
  //
  //   5. HTTP GET using cpp-httplib (httplib::Client).
  //      On connection error: return {"web_search: connection failed", true}.
  //      On non-200 status: return {"web_search: HTTP " + status, true}.
  //
  //   6. Parse JSON response; extract results array.
  //      Each result has: title, url, content (snippet).
  //
  //   7. Format up to max_results_ entries as:
  //        "1. <title> — <url>\n<content>\n\n2. ..."
  //
  //   8. return {formatted_results, false};
  //      If results empty: return {"no results found", false};

  (void)args_json;
  (void)policy;
  return {"not implemented", true};
}

}  // namespace ur
