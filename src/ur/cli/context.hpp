#pragma once

#include <string>

#include "memory/database.hpp"
#include "memory/workspace.hpp"

namespace ur {

// Shared runtime state passed to every command.
// Constructed once in main() before dispatch — no I/O at construction time.
struct Context {
  Paths paths;
  Database db;
  std::string enc_key;  // raw key bytes; empty = encryption disabled
};

// Resolve paths, attempt to load $root/keys/secret.key, wire up Database.
// Does not open the database file.
Context make_context();

}  // namespace ur
