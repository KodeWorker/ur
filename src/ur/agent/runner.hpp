#pragma once

#include <string>

#include "llm/provider.hpp"
#include "log/logger.hpp"
#include "memory/database.hpp"

namespace ur {

// Result of a single run() call.
struct RunResult {
  std::string session_id;  // ID of the newly created session
  std::string response;    // Assistant reply content
};

// Orchestrates a single-turn LLM request:
//   - Creates a session in the database
//   - Sends messages to the provider
//   - Persists user and assistant messages (encrypted if key is set by
//   provider)
//
// All references are borrowed from Context and must outlive the Runner.
class Runner {
 public:
  Runner(Database& db, Logger& logger);

  // Execute one request/response turn.
  // prompt:        user message text
  // system_prompt: injected as a "system" message if non-empty
  // model:         model name passed to provider; resolved with precedence:
  //                  --model=<name>  >  UR_LLM_MODEL  >  empty
  // provider:      called to produce the assistant response
  // token_cb:      if non-null, use provider.stream() and deliver content
  //                chunks via this callback; otherwise use provider.complete()
  // reasoning_cb:  if non-null, deliver reasoning chunks via this callback
  // Returns RunResult on success; throws std::runtime_error on failure.
  RunResult run(const std::string& prompt, const std::string& system_prompt,
                const std::string& model, Provider& provider,
                const TokenCallback& token_cb = nullptr,
                const TokenCallback& reasoning_cb = nullptr);

 private:
  // Generate a random 32-char hex ID using RAND_bytes() (OpenSSL).
  // Used for both session IDs and message IDs.
  static std::string generate_id();

  Database& db_;
  Logger& logger_;
};

}  // namespace ur
