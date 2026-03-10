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

// ur run <prompt> [--model=<name>] [--system-prompt=<file>] [--allow-all]
// Sends a single-shot request to the LLM provider, persists the session and
// messages, and prints the assistant response to stdout.
int cmd_run(Context& ctx, int argc, char** argv);

// ur chat [--continue=<id>] [--model=<name>] [--system-prompt=<file>]
// Opens an interactive ftxui TUI session. Blocks until the user exits.
int cmd_chat(Context& ctx, int argc, char** argv);

// ur history [<id>]
// No id: lists all sessions (id, title, created_at, model).
// With id: prints all messages for that session in order.
int cmd_history(Context& ctx, int argc, char** argv);

// ur persona
// Prints all accumulated persona key-value pairs.
int cmd_persona(Context& ctx, int argc, char** argv);

}  // namespace ur
