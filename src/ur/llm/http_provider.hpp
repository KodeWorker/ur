#pragma once

#include <string>
#include <vector>

#include "llm/provider.hpp"

namespace ur {

// Sends requests to any OpenAI-compatible HTTP server via cpp-httplib.
// Reads UR_LLM_BASE_URL and UR_LLM_API_KEY from environment at construction.
class HttpProvider : public Provider {
 public:
  // base_url: e.g. "http://localhost:8080" or "http://localhost:11434"
  // api_key:  sent as "Authorization: Bearer <key>"; may be empty for local
  //           servers that do not require authentication.
  explicit HttpProvider(std::string base_url, std::string api_key = {});

  // GET /props (llama.cpp) to retrieve n_ctx; falls back to zeroed ServerInfo
  // if the endpoint is absent or returns an unexpected format.
  // Never throws.
  ServerInfo server_info() override;

  // POST /v1/chat/completions; returns content + usage token counts.
  // Throws std::runtime_error on network error or non-200 response.
  CompletionResult complete(const std::vector<Message>& messages,
                            const std::string& model) override;

 private:
  std::string base_url_;
  std::string api_key_;
};

// Construct HttpProvider from environment variables.
// UR_LLM_BASE_URL defaults to "http://localhost:8080" if unset.
// UR_LLM_API_KEY defaults to empty string if unset.
// Model name is NOT read here — it is resolved in cmd_run / Runner:
//   --model=<name>  >  UR_LLM_MODEL  >  empty (server picks default)
HttpProvider make_http_provider();

}  // namespace ur
