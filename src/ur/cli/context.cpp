#include "context.hpp"

#include <string>

#include "log/logger.hpp"
#include "memory/crypto.hpp"

namespace ur {

Context make_context() {
  Paths paths = resolve_paths();
  std::string enc_key = load_key(paths.keys / "secret.key");
  // Database receives the key but does not open the file yet.
  // Logger is lazy — log file not created until first write.
  return Context{paths, Database(paths.database / "ur.db", enc_key), enc_key,
                 Logger(paths.log, log_level_from_env())};
}

}  // namespace ur
