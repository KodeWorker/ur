#include <gtest/gtest.h>

// cpp-httplib placed before C++ system headers (cpplint treats .h as C system).
#include <httplib.h>

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "llm/http_provider.hpp"

// Minimal valid OpenAI-style response body used by default handlers.
const char kValidResponse[] =
    R"({"choices":[{"message":{"role":"assistant","content":"hi there"}}]})";

namespace {

// Fixture: starts a loopback httplib::Server on a random port in SetUp()
// and stops it in TearDown(). Tests override handler_ to control responses.
class HttpProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Dispatch all POST requests through handler_ so individual tests can
    // swap it out without re-registering routes.
    svr_.Post("/v1/chat/completions",
              [this](const httplib::Request& req, httplib::Response& res) {
                std::lock_guard<std::mutex> lock(handler_mu_);
                handler_(req, res);
              });
    port_ = svr_.bind_to_any_port("localhost");
    thread_ = std::thread([this]() { svr_.listen_after_bind(); });
    base_url_ = "http://localhost:" + std::to_string(port_);
  }

  void TearDown() override {
    svr_.stop();
    if (thread_.joinable()) thread_.join();
  }

  // Replace the active request handler before calling provider.complete().
  // Guarded by handler_mu_ — the server thread reads handler_ on each request.
  void set_handler(
      std::function<void(const httplib::Request&, httplib::Response&)> h) {
    std::lock_guard<std::mutex> lock(handler_mu_);
    handler_ = std::move(h);
  }

  httplib::Server svr_;
  int port_ = 0;
  std::thread thread_;
  std::string base_url_;

  std::mutex handler_mu_;
  // Default handler: always returns a valid 200 response.
  std::function<void(const httplib::Request&, httplib::Response&)> handler_ =
      [](const httplib::Request&, httplib::Response& res) {
        res.set_content(kValidResponse, "application/json");
      };
};

}  // namespace

TEST_F(HttpProviderTest, CompletePostsToCorrectEndpoint) {
  bool called = false;
  std::string received_body;
  set_handler([&](const httplib::Request& req, httplib::Response& res) {
    called = true;
    received_body = req.body;
    res.set_content(kValidResponse, "application/json");
  });

  ur::HttpProvider provider(base_url_);
  provider.complete({{"user", "hello"}}, "");

  EXPECT_TRUE(called);
  EXPECT_NE(received_body.find("messages"), std::string::npos);
  EXPECT_NE(received_body.find("hello"), std::string::npos);
}

TEST_F(HttpProviderTest, CompleteReturnsAssistantContent) {
  ur::HttpProvider provider(base_url_);
  std::string result = provider.complete({{"user", "hello"}}, "");
  EXPECT_EQ(result, "hi there");
}

TEST_F(HttpProviderTest, CompleteThrowsOnNon200Response) {
  set_handler([](const httplib::Request&, httplib::Response& res) {
    res.status = 500;
    res.set_content("internal error", "text/plain");
  });

  ur::HttpProvider provider(base_url_);
  EXPECT_THROW(provider.complete({{"user", "hello"}}, ""), std::runtime_error);
}

TEST_F(HttpProviderTest, CompleteThrowsOnConnectionFailure) {
  // Point at a port with no server running.
  ur::HttpProvider provider("http://localhost:1");
  EXPECT_THROW(provider.complete({{"user", "hello"}}, ""), std::runtime_error);
}

TEST_F(HttpProviderTest, CompleteIncludesAuthHeader) {
  std::string auth_header;
  set_handler([&](const httplib::Request& req, httplib::Response& res) {
    if (req.has_header("Authorization")) {
      auth_header = req.get_header_value("Authorization");
    }
    res.set_content(kValidResponse, "application/json");
  });

  ur::HttpProvider provider(base_url_, "test-key");
  provider.complete({{"user", "hello"}}, "");

  EXPECT_EQ(auth_header, "Bearer test-key");
}

TEST(MakeHttpProviderTest, ReadsBaseUrlFromEnv) {
  setenv("UR_LLM_BASE_URL", "http://localhost:19999", 1);
  EXPECT_NO_THROW(ur::make_http_provider());
  unsetenv("UR_LLM_BASE_URL");
}
