#include "persona_updater.hpp"

#include <algorithm>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

// nlohmann/json — JSON parsing for persona extraction response.
#include <nlohmann/json.hpp>

namespace ur {

PersonaUpdater::PersonaUpdater(Database& db, Provider& provider, Logger& logger,
                               std::string model)
    : db_(db), provider_(provider), logger_(logger), model_(std::move(model)) {}

// static
bool PersonaUpdater::is_meaningful(const std::vector<Message>& context,
                                   const std::string& user_msg) {
  if (user_msg.size() <= 50) return false;
  return std::count_if(context.begin(), context.end(), [](const auto& m) {
           return m.role == "user" || m.role == "assistant";
         }) >= 6;
}

void PersonaUpdater::maybe_update(const std::vector<Message>& context,
                                  const std::string& user_msg,
                                  const std::string& assistant_msg) {
  if (!is_meaningful(context, user_msg)) return;

  try {
    std::vector<Message> msgs = {
        {"system",
         "Extract stable facts about the user from the exchange below. "
         "Return a flat JSON object {\"key\": \"value\", ...} using short "
         "lowercase keys (e.g. name, timezone, interests). "
         "Return {} if nothing worth persisting."},
        {"user", "[User]: " + user_msg + "\n[Assistant]: " + assistant_msg}};

    const std::string raw = provider_.complete(msgs, model_).content;

    auto j = nlohmann::json::parse(raw);
    if (!j.is_object()) return;

    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    for (auto it = j.begin(); it != j.end(); ++it) {
      if (!it.key().empty() && it.value().is_string())
        db_.upsert_persona(it.key(), it.value().get<std::string>(), now);
    }
  } catch (const std::exception& e) {
    logger_.error(std::string("persona extraction failed: ") + e.what());
  }
}

}  // namespace ur
