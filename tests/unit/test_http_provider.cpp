#include <gtest/gtest.h>

#include "llm/http_provider.hpp"

// These tests spin up a loopback httplib server to avoid needing a real LLM.
// See cpp-httplib README for the httplib::Server usage pattern.

// TODO: add fixture that starts a loopback httplib::Server in a background
// thread on a random port in SetUp() and stops it in TearDown().

TEST(HttpProviderTest, CompletePostsToCorrectEndpoint) {
  // TODO:
  // 1. Start loopback server; register handler on POST /v1/chat/completions
  //    that records the received request body.
  // 2. Construct HttpProvider("http://localhost:<port>").
  // 3. Call provider.complete({{"user","hello"}}, "").
  // 4. Assert the handler was called.
  // 5. Assert the request body contains "messages" and "hello".
  GTEST_SKIP() << "not implemented";
}

TEST(HttpProviderTest, CompleteReturnsAssistantContent) {
  // TODO:
  // 1. Start loopback server; respond with a valid OpenAI-style JSON body:
  //    {"choices":[{"message":{"role":"assistant","content":"hi there"}}]}
  // 2. Call provider.complete({{"user","hello"}}, "").
  // 3. Assert return value == "hi there".
  GTEST_SKIP() << "not implemented";
}

TEST(HttpProviderTest, CompleteThrowsOnNon200Response) {
  // TODO:
  // 1. Start loopback server; respond with status 500.
  // 2. Assert provider.complete(...) throws std::runtime_error.
  GTEST_SKIP() << "not implemented";
}

TEST(HttpProviderTest, CompleteThrowsOnConnectionFailure) {
  // TODO:
  // 1. Construct HttpProvider pointing at a port with no server running.
  // 2. Assert provider.complete(...) throws std::runtime_error.
  GTEST_SKIP() << "not implemented";
}

TEST(HttpProviderTest, CompleteIncludesAuthHeader) {
  // TODO:
  // 1. Start loopback server; record request headers.
  // 2. Construct HttpProvider("http://localhost:<port>", "test-key").
  // 3. Call provider.complete(...).
  // 4. Assert Authorization header == "Bearer test-key".
  GTEST_SKIP() << "not implemented";
}

TEST(MakeHttpProviderTest, ReadsBaseUrlFromEnv) {
  // TODO:
  // 1. setenv("UR_LLM_BASE_URL", "http://localhost:9999", 1).
  // 2. auto p = make_http_provider().
  // 3. Assert p is constructable without throwing.
  // 4. unsetenv("UR_LLM_BASE_URL").
  GTEST_SKIP() << "not implemented";
}
