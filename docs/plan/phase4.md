# Phase 4 — Tool System and Sandbox Tier 1

**Goal**: agents can call tools loaded from `$root/tools/`. Tool execution is confined to `$root/workspace/` by default.

**Prerequisite**: Phase 3 complete.

## Deliverables

- Tool plugin loading from `$root/tools/`
- Tool-calling loop integrated into `runner` and `chat`
- Sandbox tier 1: workspace path constraint enforced
- `--tools=<file>` flag for `ur run` and `ur chat`
- `--allow-all` bypasses sandbox

## Source Files to Create

```
src/ur/tools/tool.hpp               Tool interface (name, description, input schema, execute)
src/ur/tools/loader.cpp/.hpp        Discover and load plugins from $root/tools/
src/ur/tools/sandbox.cpp/.hpp       Tier 1 path enforcement
src/ur/tools/executor.cpp/.hpp      Route tool calls from LLM → plugin → sandbox → result
tests/unit/test_loader.cpp
tests/unit/test_sandbox.cpp
tests/unit/test_executor.cpp
```

## Tool Interface

```cpp
// src/ur/tools/tool.hpp
struct ToolCall  { std::string name; std::string args_json; };
struct ToolResult { std::string content; bool is_error; };

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::string input_schema_json() const = 0;
    virtual ToolResult execute(const std::string& args_json, const SandboxPolicy&) = 0;
};
```

## Plugin Loading

Plugins in `$root/tools/` can be:
- Shared libraries (`.so` / `.dll`) that export a `create_tool()` factory function
- Executable scripts (any language) that follow a JSON stdio protocol

## Tool-Calling Loop

```
1. Inject tool definitions into the system prompt / tool list
2. Call LLM
3. If response contains tool calls:
   a. For each call:
      - If NOT --allow-all: request human audit (see below); on rejection,
        append ToolResult { is_error: true, "rejected by user" } and skip execution
      - If approved (or --allow-all): resolve tool → enforce sandbox → execute
   b. Collect all results, append to context
   c. Call LLM again with results
4. Repeat until no tool calls in response
5. Return final text response
```

## Human Audit

Without `--allow-all`, every tool call requires explicit user approval before
execution. The audit surface differs by command:

- **`ur chat`** — ftxui modal block shows tool name + formatted args; user
  presses `y` to approve or `n` to reject inline in the TUI
- **`ur run`** — prints tool name + args to stderr and reads a `y/n` line from
  stdin; non-interactive environments (piped stdin) treat it as rejection

Approval/rejection is recorded as a `"tool_audit"` message role in the session
for auditability. `--allow-all` bypasses the audit entirely.

## Sandbox Tier 1: Workspace Constraint

All file paths in tool arguments are validated before execution:

- Resolve to absolute path
- Reject any path outside `$root/workspace/`
- Return a `ToolResult { is_error: true }` with a clear message on violation

`--allow-all` disables path validation.

## `--tools` Flag

Accepts a JSON file listing which tools (by name) are enabled for this invocation. If omitted, all loaded tools are available.

## Acceptance Criteria

- [ ] Tools in `$root/tools/` are discovered and registered at startup
- [ ] LLM tool calls are routed through the executor and results returned to LLM
- [ ] Tier 1 sandbox blocks any path outside `$root/workspace/`
- [ ] `--allow-all` allows arbitrary paths and bypasses human audit
- [ ] Without `--allow-all`, tool calls are held for user approval before execution
- [ ] Rejected tool calls return an error result to the LLM without executing
- [ ] Invalid tool name in LLM response returns a clear error result (no crash)
