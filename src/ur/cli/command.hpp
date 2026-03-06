#pragma once

#include "context.hpp"

namespace ur {

// ur init
// Creates workspace directories and initialises the SQLite schema.
int cmd_init(Context& ctx, int argc, char** argv);

// ur clean [--database|--workspace]
// No flag: removes both workspace contents and drops all database tables.
// --workspace: removes workspace/ contents only.
// --database:  drops all database tables only.
int cmd_clean(Context& ctx, int argc, char** argv);

}  // namespace ur
