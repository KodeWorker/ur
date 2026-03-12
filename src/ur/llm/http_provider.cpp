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
    base_url = "http://localhost:8080";
  }
  const char* api_key = std::getenv("UR_LLM_API_KEY");
  if (api_key == nullptr) {
    api_key = "";
  }
  return HttpProvider(base_url, api_key);
}

ServerInfo HttpProvider::server_info() {
  // GET /props — llama.cpp specific; returns { "n_ctx": <int>, ... }.
  // Falls back to zeroed ServerInfo for any other server or on any error.
  ServerInfo info;
  try {
    httplib::Client client(base_url_);
    if (!api_key_.empty()) {
      client.set_default_headers({{"Authorization", "Bearer " + api_key_}});
    }
    auto res = client.Get("/props");
    if (res == nullptr || res->status != 200) return info;
    auto j = nlohmann::json::parse(res->body);
    if (j.contains("n_ctx") && j["n_ctx"].is_number_integer()) {
      info.context_length = j["n_ctx"].get<int>();
    }
  } catch (...) {
  }
  return info;
}

CompletionResult HttpProvider::complete(const std::vector<Message>& messages,
                                        const std::string& model) {
  // Build request JSON.
  nlohmann::json request_json;
  if (!model.empty()) {
    request_json["model"] = model;
  }
  request_json["messages"] = nlohmann::json::array();
  for (const auto& msg : messages) {
    request_json["messages"].push_back(
        {{"role", msg.role}, {"content", msg.content}});
  }
  // Client setup.
  const std::string body = request_json.dump();
  httplib::Client client(base_url_);
  const char* conn_timeout = std::getenv("UR_LLM_CONNECTION_TIMEOUT");
  const char* read_timeout = std::getenv("UR_LLM_READ_TIMEOUT");
  client.set_connection_timeout(conn_timeout ? std::stoi(conn_timeout) : 10, 0);
  if (read_timeout) {
    client.set_read_timeout(std::stoi(read_timeout), 0);
  }
  if (!api_key_.empty()) {
    client.set_default_headers({{"Authorization", "Bearer " + api_key_}});
  }
  // POST request.
  auto res = client.Post("/v1/chat/completions", body, "application/json");
  if (res == nullptr) {
    throw std::runtime_error("connection failed");
  }
  if (res->status != 200) {
    throw std::runtime_error("HTTP error " + std::to_string(res->status) +
                             ": " + res->body);
  }
  try {
    nlohmann::json j = nlohmann::json::parse(res->body);
    CompletionResult result;
    auto& msg = j["choices"][0]["message"];
    result.content = msg["content"].get<std::string>();
    // Dedicated reasoning field (Qwen3, some llama.cpp builds).
    if (msg.contains("reasoning_content") &&
        msg["reasoning_content"].is_string()) {
      result.reasoning_content = msg["reasoning_content"].get<std::string>();
    }
    if (j.contains("usage") && j["usage"].is_object()) {
      auto& u = j["usage"];
      if (u.contains("prompt_tokens") && u["prompt_tokens"].is_number_integer())
        result.prompt_tokens = u["prompt_tokens"].get<int>();
      if (u.contains("completion_tokens") &&
          u["completion_tokens"].is_number_integer())
        result.completion_tokens = u["completion_tokens"].get<int>();
    }
    return result;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(std::string("response parsing error: ") +
                             e.what());
  }
}

}  // namespace ur
