#include "persona_updater.hpp"

#include <algorithm>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

// nlohmann/json — JSON parsing for persona extraction response.
#include <nlohmann/json.hpp>

namespace ur {

PersonaUpdater::PersonaUpdater(Database& db, Provider& provider,
                               std::string model)
    : db_(db), provider_(provider), model_(std::move(model)) {}

// static
bool PersonaUpdater::is_meaningful(const std::vector<Message>& context,
                                   const std::string& user_msg) {
  // TODO: count messages in context whose role is "user" or "assistant";
  //       return true iff count >= 6 AND user_msg.size() > 50.
  (void)context;
  (void)user_msg;
  return false;
}

void PersonaUpdater::maybe_update(const std::vector<Message>& context,
                                  const std::string& user_msg,
                                  const std::string& assistant_msg) {
  // cppcheck-suppress knownConditionTrueFalse
  if (!is_meaningful(context, user_msg)) return;

  // TODO:
  // 1. Build extraction prompt:
  //      system: "Extract stable facts about the user from the exchange below.
  //               Return a flat JSON object {\"key\": \"value\", ...} using
  //               short lowercase keys (e.g. name, timezone, interests).
  //               Return {} if nothing worth persisting."
  //      user:   "[User]: <user_msg>\n[Assistant]: <assistant_msg>"
  //
  // 2. provider_.complete(extraction_messages, model_) → raw string.
  //
  // 3. nlohmann::json::parse(raw); for each string key/value:
  //      db_.upsert_persona(key, value, now).
  //
  // 4. Catch and swallow all exceptions — persona extraction is best-effort.
  (void)assistant_msg;
}

}  // namespace ur
