#include "context.hpp"

#include "memory/crypto.hpp"

namespace ur {

Context make_context() {
  Paths paths = resolve_paths();
  std::string enc_key = load_key(paths.keys / "secret.key");
  // Database receives the key but does not open the file yet.
  return Context{paths, Database(paths.database / "ur.db", enc_key), enc_key};
}

} // namespace ur
