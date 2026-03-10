#include "context.hpp"

#include <string>

#include "log/logger.hpp"
#include "memory/crypto.hpp"

namespace ur {

Context make_context() {
  Paths paths = resolve_paths();
  std::string enc_key = load_key(paths.key / "secret.key");
  // Empty key = plaintext mode (no key file present). A non-empty key must be
  // exactly 32 bytes for AES-256-GCM; anything else is a misconfiguration.
  if (!enc_key.empty() && enc_key.size() != 32) {
    throw std::runtime_error("encrypt: key must be 32 bytes for AES-256-GCM");
  }
  // Database receives the key but does not open the file yet.
  // Logger is lazy — log file not created until first write.
  return Context{paths, Database(paths.database / "ur.db", enc_key), enc_key,
                 Logger(paths.logs, log_level_from_env())};
}

}  // namespace ur
