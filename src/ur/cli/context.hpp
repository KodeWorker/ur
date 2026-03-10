#pragma once

#include <string>

#include "log/logger.hpp"
#include "memory/database.hpp"
#include "memory/workspace.hpp"

namespace ur {

// Shared runtime state passed to every command.
// Constructed once in main() before dispatch — no I/O at construction time.
struct Context {
  Paths paths;
  Database db;
  std::string enc_key;  // 32 raw bytes of the AES-256-GCM key
  Logger logger;
};

// Resolve paths, load $root/key/secret.key (throws if absent), wire up
// Database. Does not open the database file.
Context make_context();

}  // namespace ur
