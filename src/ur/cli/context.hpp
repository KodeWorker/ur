#pragma once

#include "memory/database.hpp"
#include "memory/workspace.hpp"

namespace ur {

// Shared runtime state passed to every command.
// Constructed once in main() before dispatch — no I/O at construction time.
struct Context {
  Paths paths;
  Database db;
};

// Resolve paths and wire up the Database object.
// Does not touch the filesystem or open the database.
Context make_context();

} // namespace ur
