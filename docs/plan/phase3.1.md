# Phase 3.1 — `ur run` Flag Redesign for Sub-Agent Invocation

**Goal**: Redesign `ur run` flags so that a parent agent can programmatically
invoke it as a sub-agent, passing system prompts and tool restrictions inline
without writing temp files or abusing environment variables.

**Prerequisite**: Phase 3 complete.

## Motivation

In Phase 4, the tool system will include a built-in `run_agent` tool that lets
an agent spawn a sub-agent by calling `ur run` as a subprocess. For this to
work cleanly, `ur run` must accept its configuration in a form that is
convenient for programmatic callers — not just interactive users.

Two problems with the current flags:

1. `--system-prompt=<path>` requires a file on disk. A parent agent with an
   in-memory system prompt would have to write a temp file, pass its path, and
   clean up. That is fragile and racy.

2. There is no way to restrict which tools the sub-agent may use. Without
   filtering, a sub-agent inherits the full tool set and could cause unbounded
   side effects.

### Design principle: configuration vs selection

Tool plugin systems split two concerns:

| Concern | Question | Scope |
|---------|----------|-------|
| **Configuration** | What does this tool do, how is it constrained? | Persistent — `$root/tools/tools.json` |
| **Selection** | Which tools are active this invocation? | Ephemeral — CLI flags |

`--tools=<path>` (previously planned) conflated both concerns into a single
per-invocation JSON file. Instead, persistent configuration lives in
`$root/tools/tools.json` (Phase 4), and per-invocation selection uses
`--allow`/`--deny`/`--no-tools` inline flags. `--tools` is dropped.

### Why not environment variables for selection?

Environment variables are process-global and inherited transitively. Nesting
agents (agent → sub-agent → sub-sub-agent) would corrupt each other's settings
unless every level carefully saves and restores env state. CLI flags scope
cleanly to the invocation they were passed to.

## Deliverables

- `--system-prompt` accepts an inline string **or** a file path (with `@` prefix)
- `--allow=<tool,...>` whitelist — only these tools are active for this invocation
- `--deny=<tool,...>` blacklist — these tools are excluded
- `--no-tools` — disable all tool loading
- Recursion guard via `UR_AGENT_DEPTH` / `UR_MAX_AGENT_DEPTH`
- `README.md` and `docs/plan/phase4.md` updated to remove `--tools=<path>`

## Flag Specification

### Full `ur run` signature (after this phase)

```
ur run <prompt>
  [--model=<name>]
  [--system-prompt=<text>|@<path>]
  [--allow=<tool,...>]
  [--deny=<tool,...>]
  [--no-tools]
  [--allow-all]
```

### `--system-prompt`

```
--system-prompt=<text>     inline string, used directly
--system-prompt=@<path>    read from file at <path>
```

Detected by whether the value starts with `@`. A bare `@` with no path is an
error. **Backward compatibility**: the old `--system-prompt=<path>` (no `@`)
becomes an inline string. Callers using file paths must add `@`. The old form
was not yet in a released version; no migration path required.

### `--allow=<tool,...>`

Comma-separated list of tool names. The tool loader only registers the named
tools. Unknown names are silently ignored. Mutually exclusive with `--deny`.

### `--deny=<tool,...>`

Comma-separated list of tool names to exclude. All other loaded tools remain
available. Mutually exclusive with `--allow`.

### `--no-tools`

Disables tool loading entirely regardless of `$root/tools/` contents. Takes
precedence over `--allow` and `--deny` if combined.

### `--allow-all`

Unchanged: bypasses sandbox path enforcement and skips the human audit prompt.
Independent of `--allow`/`--deny` — `--allow-all` lifts sandbox constraints
but does not override the tool whitelist/blacklist.

## `run_agent` Tool (Phase 4 preview)

The built-in tool that the Phase 4 executor will register:

```
tool name:  run_agent
arguments:
  prompt         string   required   the sub-task prompt
  system_prompt  string   optional   inline; inherit parent's if omitted
  model          string   optional   inherit parent's model if omitted
  allow          string   optional   comma-separated tool whitelist
  deny           string   optional   comma-separated tool blacklist
  no_tools       bool     optional   disable all tools in sub-agent
```

The executor builds the `ur run` invocation from these arguments:

```
ur run "<prompt>"
  [--model=<model>]
  [--system-prompt=<system_prompt>]    # inline — no temp file needed
  [--allow=<allow> | --deny=<deny> | --no-tools]
```

Stdout is captured and returned as the tool result. Each sub-agent creates its
own session row in the shared database. The parent's context window is
unaffected.

### Recursion guard

The executor sets `UR_AGENT_DEPTH=<n>` in the subprocess environment. `ur run`
reads this at startup, increments it, and exits with an error before calling
the provider if the value exceeds `UR_MAX_AGENT_DEPTH` (default: 4).

## Source Files to Modify

```
src/ur/cli/command.cpp      cmd_run: parse @-prefix for --system-prompt;
                            parse --allow, --deny, --no-tools;
                            read UR_AGENT_DEPTH; enforce UR_MAX_AGENT_DEPTH
```

Tool filtering flags are parsed and stored but are no-ops in Phase 3.1 — the
tool loader does not exist until Phase 4. This avoids touching the flag-parsing
layer again when Phase 4 wires filtering to the loader.

## Acceptance Criteria

- [ ] `ur run "hello" --system-prompt="You are a pirate."` passes the inline
      system prompt to the provider without creating any temp file
- [ ] `ur run "hello" --system-prompt=@/tmp/sys.txt` reads system prompt from file
- [ ] `--system-prompt=@nonexistent` prints `[ERROR]` and exits non-zero
- [ ] `--allow=foo,bar` is parsed without error; stored for Phase 4
- [ ] `--deny=foo` is parsed without error; stored for Phase 4
- [ ] `--no-tools` is parsed without error; stored for Phase 4
- [ ] `--allow` and `--deny` together print a usage error and exit non-zero
- [ ] `UR_AGENT_DEPTH=5` (above default limit 4) causes `ur run` to exit with
      `[ERROR] maximum agent recursion depth exceeded`
- [ ] All existing `ur run` tests continue to pass
