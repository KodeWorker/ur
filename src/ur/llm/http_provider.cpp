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

#include "env.hpp"

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
  client.set_connection_timeout(env_int("UR_LLM_CONNECTION_TIMEOUT", 10), 0);
  // immediately — never pass 0 as an actual timeout value.
  client.set_read_timeout(env_int("UR_LLM_READ_TIMEOUT", 300), 0);

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

void HttpProvider::stream(const std::vector<Message>& messages,
                          const std::string& model,
                          const TokenCallback& token_cb,
                          const TokenCallback& reasoning_cb) {
  // Build request JSON with stream:true.
  nlohmann::json req_json;
  if (!model.empty()) req_json["model"] = model;
  req_json["stream"] = true;
  req_json["messages"] = nlohmann::json::array();
  for (const auto& msg : messages) {
    req_json["messages"].push_back(
        {{"role", msg.role}, {"content", msg.content}});
  }
  // Client setup (same timeout logic as complete()).
  httplib::Client client(base_url_);
  client.set_connection_timeout(env_int("UR_LLM_CONNECTION_TIMEOUT", 10), 0);
  // immediately — never pass 0 as an actual timeout value.
  client.set_read_timeout(env_int("UR_LLM_READ_TIMEOUT", 300), 0);

  if (!api_key_.empty()) {
    client.set_default_headers({{"Authorization", "Bearer " + api_key_}});
  }
  // Use httplib Request with response_handler + content_receiver for SSE.
  httplib::Request req;
  req.method = "POST";
  req.path = "/v1/chat/completions";
  req.body = req_json.dump();
  req.set_header("Content-Type", "application/json");

  bool is_error = false;
  int status_code = 0;
  std::string error_body;
  std::string line_buf;

  req.response_handler = [&](const httplib::Response& res) -> bool {
    status_code = res.status;
    is_error = (res.status != 200);
    return true;
  };

  req.content_receiver = [&](const char* data, size_t len, uint64_t /*off*/,
                             uint64_t /*total*/) -> bool {
    if (is_error) {
      error_body.append(data, len);
      return true;
    }
    line_buf.append(data, len);
    size_t pos;
    while ((pos = line_buf.find('\n')) != std::string::npos) {
      std::string line = line_buf.substr(0, pos);
      line_buf.erase(0, pos + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty() || line == "data: [DONE]") continue;
      if (line.rfind("data: ", 0) != 0) continue;
      try {
        auto j = nlohmann::json::parse(line.substr(6));
        auto& delta = j["choices"][0]["delta"];
        if (token_cb && delta.contains("content") &&
            delta["content"].is_string()) {
          const std::string chunk = delta["content"].get<std::string>();
          if (!chunk.empty()) token_cb(chunk);
        }
        if (reasoning_cb && delta.contains("reasoning_content") &&
            delta["reasoning_content"].is_string()) {
          const std::string chunk =
              delta["reasoning_content"].get<std::string>();
          if (!chunk.empty()) reasoning_cb(chunk);
        }
      } catch (...) {
      }
    }
    return true;
  };

  auto res = client.send(req);
  if (!res) throw std::runtime_error("connection failed");
  if (is_error) {
    throw std::runtime_error("HTTP error " + std::to_string(status_code) +
                             ": " + error_body);
  }
}

}  // namespace ur
