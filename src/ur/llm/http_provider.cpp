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
  const char* base_url = std::getenv("UR_LLM_BASE_URL");
  if (base_url == nullptr || std::string(base_url).empty()) {
    base_url = "http://localhost:8000";
  }
  const char* api_key = std::getenv("UR_LLM_API_KEY");
  if (api_key == nullptr) {
    api_key = "";
  }
  return HttpProvider(base_url, api_key);
}

std::string HttpProvider::complete(const std::vector<Message>& messages,
                                   const std::string& model) {
  // Build request JSON
  nlohmann::json request_json;
  if (!model.empty()) {
    request_json["model"] = model;
  }
  request_json["messages"] = nlohmann::json::array();
  for (const auto& msg : messages) {
    request_json["messages"].push_back(
        {{"role", msg.role}, {"content", msg.content}});
  }
  // Client setup and request
  const std::string body = request_json.dump();
  auto scheme_end = base_url_.find("://");
  if (scheme_end == std::string::npos)
    throw std::runtime_error("complete: invalid base_url: " + base_url_);
  auto path_start =
      base_url_.find('/', scheme_end + 3);  // first '/' after the host
  const std::string host =
      base_url_.substr(0, path_start);  // "http://localhost:8000"
  const std::string path_prefix =
      (path_start != std::string::npos) ? base_url_.substr(path_start) : "";
  const std::string endpoint = path_prefix + "/v1/chat/completions";
  httplib::Client client(host);
  if (!api_key_.empty()) {
    client.set_default_headers({{"Authorization", "Bearer " + api_key_}});
  }
  // POST request
  auto res = client.Post(endpoint, body, "application/json");
  if (res == nullptr) {
    throw std::runtime_error("connection failed");
  }
  if (res->status != 200) {
    throw std::runtime_error("HTTP error " + std::to_string(res->status) +
                             ": " + res->body);
  }
  try {
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    return response_json["choices"][0]["message"]["content"].get<std::string>();
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(std::string("response parsing error: ") +
                             e.what());
  }
}

}  // namespace ur
