#include "runner.hpp"

#include <openssl/rand.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ur {

Runner::Runner(Database& db, const std::string& enc_key, Logger& logger)
    : db_(db), enc_key_(enc_key), logger_(logger) {}

std::string Runner::generate_id() {
  // TODO:
  // 1. Declare: unsigned char buf[16];
  // 2. Call RAND_bytes(buf, sizeof(buf)); throw on return value != 1.
  // 3. Convert buf to a 32-char lowercase hex string using std::ostringstream
  //    and std::hex + std::setfill('0') + std::setw(2).
  // 4. Return the hex string.
  (void)0;
  throw std::runtime_error("Runner::generate_id: not implemented");
}

RunResult Runner::run(const std::string& prompt,
                      const std::string& system_prompt,
                      const std::string& model, Provider& provider) {
  // TODO:
  // 1. db_.init_schema()  — lazy-open and create tables (idempotent).
  //
  // 2. Generate IDs:
  //      session_id  = generate_id()
  //      user_msg_id = generate_id()
  //      asst_msg_id = generate_id()
  //
  // 3. Get current Unix timestamp:
  //      int64_t now = static_cast<int64_t>(std::time(nullptr));
  //
  // 4. Insert session row:
  //      INSERT INTO session (id, title, model, created_at, updated_at)
  //      VALUES (session_id, title, model, now, now)
  //      where title = first 60 chars of prompt.
  //    Use sqlite3_prepare_v2 / sqlite3_bind_text / sqlite3_step pattern
  //    (see database.cpp for the style to follow).
  //
  // 5. Build messages vector:
  //      if system_prompt is non-empty: push {role="system",
  //      content=system_prompt} push {role="user", content=prompt}
  //
  // 6. Call provider.complete(messages, model) to get response string.
  //    Log the call at debug level before and after.
  //
  // 7. Insert user message:
  //      INSERT INTO message (id, session_id, role, content, created_at)
  //      VALUES (user_msg_id, session_id, "user", enc_(prompt), now)
  //    enc_: encrypt content using enc_key_ if set (call ur::encrypt()).
  //
  // 8. Insert assistant message:
  //      VALUES (asst_msg_id, session_id, "assistant", enc_(response), now)
  //
  // 9. Return RunResult{session_id, response}.
  (void)prompt;
  (void)system_prompt;
  (void)model;
  (void)provider;
  throw std::runtime_error("Runner::run: not implemented");
}

}  // namespace ur
