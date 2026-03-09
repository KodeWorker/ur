#include "http_provider.hpp"

// cpp-httplib — included via FetchContent.
// CPPHTTPLIB_OPENSSL_SUPPORT is set by httplib::httplib CMake target.
// Placed before C++ system headers: cpplint treats .h files as C system
// headers.
#include <httplib.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// nlohmann/json — included via FetchContent for request building and response
// parsing.
#include <nlohmann/json.hpp>

namespace ur {

HttpProvider::HttpProvider(std::string base_url, std::string api_key)
    : base_url_(std::move(base_url)), api_key_(std::move(api_key)) {}

HttpProvider make_http_provider() {
  // TODO:
  // 1. Read UR_LLM_BASE_URL from std::getenv; default to
  //    "http://localhost:8080" if unset or empty.
  // 2. Read UR_LLM_API_KEY from std::getenv; default to empty string.
  // 3. Return HttpProvider(base_url, api_key).
  throw std::runtime_error("make_http_provider: not implemented");
}

std::string HttpProvider::complete(const std::vector<Message>& messages,
                                   const std::string& model) {
  // TODO:
  // 1. Build the JSON request body:
  //    {
  //      "model": model,          // omit key if model is empty
  //      "messages": [
  //        {"role": msg.role, "content": msg.content}, ...
  //      ]
  //    }
  //    Use nlohmann::json to construct the object and call .dump().
  //
  // 2. Split base_url_ into host and path prefix using httplib::detail or
  //    manual string parsing (find "://", then first "/").
  //    e.g. "http://localhost:8080" → host="http://localhost:8080", path=""
  //
  // 3. Create httplib::Client client(host).
  //    If api_key_ is non-empty, set the Authorization header:
  //      client.set_default_headers({{"Authorization", "Bearer " +
  //      api_key_}});
  //
  // 4. POST to path + "/v1/chat/completions" with body and content-type
  //    "application/json".
  //    auto res = client.Post(endpoint, body, "application/json");
  //
  // 5. Check res: if nullptr, throw std::runtime_error with "connection
  //    failed".
  //    If res->status != 200, throw std::runtime_error with status + body.
  //
  // 6. Parse res->body as JSON. Extract:
  //    response_json["choices"][0]["message"]["content"].get<std::string>()
  //    Throw on missing keys or type errors.
  //
  // 7. Return the extracted content string.
  (void)messages;
  (void)model;
  throw std::runtime_error("HttpProvider::complete: not implemented");
}

}  // namespace ur
