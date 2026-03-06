#include "context.hpp"

namespace ur {

Context make_context() {
  Paths paths = resolve_paths();
  // Database is constructed with its path but not opened yet.
  return Context{paths, Database(paths.database / "ur.db")};
}

} // namespace ur
