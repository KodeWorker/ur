#include "runner.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "memory/crypto.hpp"

namespace ur {

Runner::Runner(Database& db, Logger& logger) : db_(db), logger_(logger) {}

std::string Runner::generate_id() {
  std::string bytes = random_bytes(16);
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (unsigned char c : bytes) {
    oss << std::setw(2) << static_cast<int>(c);
  }
  return oss.str();
}

RunResult Runner::run(const std::string& prompt,
                      const std::string& system_prompt,
                      const std::string& model, Provider& provider,
                      const TokenCallback& token_cb,
                      const TokenCallback& reasoning_cb) {
  if (!db_.is_open()) {
    throw std::runtime_error(
        "Runner::run: database is not open, init schema first");
  }
  std::string session_id = generate_id();
  std::string user_msg_id = generate_id();
  std::string asst_msg_id = generate_id();
  int64_t now = static_cast<int64_t>(std::time(nullptr));
  // Prepare messages for provider call (system + user).
  std::vector<Message> messages;
  if (!system_prompt.empty()) {
    messages.push_back({"system", system_prompt});
  }
  messages.push_back({"user", prompt});
  // Network call outside the transaction — no DB lock held during I/O.
  logger_.debug("calling provider: model=" + model);
  CompletionResult cr;
  if (token_cb || reasoning_cb) {
    // Wrap callbacks: forward each chunk to the caller AND accumulate into cr
    // so the full text is available for the DB write below.
    auto wrap_token = [&](const std::string& chunk) {
      cr.content += chunk;
      if (token_cb) token_cb(chunk);
    };
    auto wrap_reasoning = [&](const std::string& chunk) {
      cr.reasoning_content += chunk;
      if (reasoning_cb) reasoning_cb(chunk);
    };
    provider.stream(messages, model, wrap_token, wrap_reasoning);
  } else {
    cr = provider.complete(messages, model);
  }
  const std::string& response = cr.content;
  logger_.debug("provider returned");

  // All three writes succeed or none do.
  db_.begin();
  try {
    db_.insert_session(session_id, "", model, now, now);
    db_.insert_message(user_msg_id, session_id, "user", prompt, now);
    db_.insert_message(asst_msg_id, session_id, "assistant", response, now);
    db_.commit();
  } catch (...) {
    db_.rollback();
    throw;
  }
  return {session_id, response};
}

}  // namespace ur
