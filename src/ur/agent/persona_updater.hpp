#pragma once

#include <string>
#include <vector>

#include "llm/provider.hpp"
#include "memory/database.hpp"

namespace ur {

class PersonaUpdater {
 public:
  // provider is used for the secondary extraction LLM call.
  // model: same model as the active chat session.
  PersonaUpdater(Database& db, Provider& provider, std::string model);

  // Apply the meaningful-turn filter; if it passes, call the provider to
  // extract persona facts and upsert them into the persona table.
  //
  // context:       full in-memory message history (all roles) at time of call.
  // user_msg:      latest user message (plaintext).
  // assistant_msg: cleaned assistant response (no <think> block).
  //
  // This call is best-effort — any provider or parse error is swallowed so
  // it never crashes the chat loop.
  void maybe_update(const std::vector<Message>& context,
                    const std::string& user_msg,
                    const std::string& assistant_msg);

 private:
  // Returns true when BOTH conditions hold:
  //   - user_msg.size() > 50               (length threshold)
  //   - >= 6 "user"/"assistant" messages in context  (depth threshold)
  static bool is_meaningful(const std::vector<Message>& context,
                            const std::string& user_msg);

  Database& db_;
  Provider& provider_;
  std::string model_;
};

}  // namespace ur
