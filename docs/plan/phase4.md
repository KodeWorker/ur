# Phase 4 — Tool System and Sandbox Tier 1

**Goal**: agents can call tools loaded from `$root/tools/`. Tool execution is confined to `$root/workspace/` by default.

**Prerequisite**: Phase 3 complete.

## Deliverables

- Tool plugin loading from `$root/tools/`, configured via `$root/tools/tools.json`
- Tool-calling loop integrated into `runner` and `chat`
- Sandbox tier 1: workspace path constraint enforced
- `--allow-all` bypasses sandbox
- Built-in tools: `read_file`, `write_file`, `bash`, `web_search`, `web_fetch`, `read_image` (disabled by default — requires vision model)

## Source Files to Create

```
src/ur/tools/tool.hpp                       Tool interface (name, description, input schema, execute)
src/ur/tools/dl_compat.hpp                  dlopen/LoadLibrary portability shim (header-only)
src/ur/tools/loader.cpp/.hpp                Discover and load plugins from $root/tools/
src/ur/tools/sandbox.cpp/.hpp               Tier 1 path enforcement
src/ur/tools/executor.cpp/.hpp              Route tool calls from LLM → plugin → sandbox → result
src/ur/tools/builtin/read_file.cpp/.hpp     Read a file (workspace-constrained)
src/ur/tools/builtin/write_file.cpp/.hpp    Create or overwrite a file (workspace-constrained)
src/ur/tools/builtin/bash.cpp/.hpp          Execute a shell command (--allow-all only)
src/ur/tools/builtin/web_search.cpp/.hpp    HTTP GET against SearXNG JSON API (--allow-all only)
src/ur/tools/builtin/web_fetch.cpp/.hpp     Fetch a URL's content (--allow-all only)
src/ur/tools/builtin/read_image.cpp/.hpp    Validate and return an image path for vision models (workspace-constrained; disabled by default)
tests/unit/test_loader.cpp
tests/unit/test_sandbox.cpp
tests/unit/test_executor.cpp
tests/unit/test_builtin_tools.cpp
```

## Built-in Tools

Built-in tools are compiled into `ur_lib` and registered by `loader.cpp` before
external plugins are scanned. Each can be disabled via `tools.json` or filtered
per-invocation with `--allow` / `--deny` / `--no-tools`.

| Tool | Path constraint | Human audit | Description |
|------|----------------|-------------|-------------|
| `read_file` | workspace only | yes (unless `--allow-all`) | Read a file inside `$root/workspace/` |
| `write_file` | workspace only | yes (unless `--allow-all`) | Create or overwrite a file inside `$root/workspace/` |
| `bash` | none | yes (unless `--allow-all`) | Execute a shell command; no path restriction |
| `web_search` | none | yes (unless `--allow-all`) | Query SearXNG JSON API and return ranked results; requires `UR_SEARCH_BASE_URL` |
| `web_fetch` | none | yes (unless `--allow-all`) | Fetch the content of a URL |
| `read_image` | workspace only | yes (unless `--allow-all`) | Validate and return an image path to vision-capable models; **disabled by default** — enabled only when `tools.json` opts in AND `ServerInfo::supports_vision` is true |

## Tool Interface

```cpp
// src/ur/tools/tool.hpp
struct ToolCall {
    std::string id;         // tool_call_id from LLM — echoed in result message
    std::string name;
    std::string args_json;
};

struct ToolResult {
    std::string content;
    bool is_error;
};

struct SandboxPolicy {
    std::filesystem::path workspace_root;  // absolute path ($root/workspace/)
    bool allow_all;                        // --allow-all: bypass path + audit
};

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::string input_schema_json() const = 0;  // OpenAI JSON Schema object
    virtual bool requires_allow_all() const = 0;        // true for bash/web_*/read_image
    virtual ToolResult execute(const std::string& args_json, const SandboxPolicy&) = 0;
};
```

## Plugin Loading

Plugins in `$root/tools/` can be:
- Shared libraries (`.so` / `.dll`) exporting `ur_create_tool()` and `ur_destroy_tool()` factory functions
- Executable scripts (any language) following a JSON stdio protocol

Override semantics: an external plugin with the same name as a built-in replaces it.

### Script Plugin Protocol

```
# Describe (called once at load time):
$ ./my_tool --describe
{"name": "my_tool", "description": "...", "input_schema": {...}}

# Execute (called per tool call, args_json on stdin):
$ ./my_tool
{"content": "...", "is_error": false}
```

## `web_search` — SearXNG Backend

`web_search` sends an HTTP GET to a SearXNG instance and returns ranked results:

```
GET $UR_SEARCH_BASE_URL/search?q=<query>&format=json
```

**Setup**: SearXNG is managed externally by the user — `ur` does not start or stop it.
The recommended setup is Docker:

```bash
docker run --rm -p 8888:8080 searxng/searxng
```

Then set `UR_SEARCH_BASE_URL=http://localhost:8888` in `.env` or the environment.

If `UR_SEARCH_BASE_URL` is not set, `web_search` is skipped at loader registration time
with a warning:

```
ur: warning: web_search disabled — UR_SEARCH_BASE_URL not set
```

## `read_image` — Vision Capability Guard

`read_image` is subject to a two-layer enable check in `Loader::load()`:

| `tools.json` `enabled` | `ServerInfo::supports_vision` | Result |
|---|---|---|
| `false` | either | disabled silently |
| `true` | `false` | disabled + warning: "read_image disabled — model does not support vision" |
| `true` | `true` | enabled |

`ServerInfo` gains a `bool supports_vision` field. For llama.cpp, this is derived
from the model metadata endpoint; if unavailable, it defaults to `false`.

## Tool-Calling Loop

Tool-calling turns use `Provider::complete()` (non-streaming) since tool call JSON
must be fully assembled before execution. Final text response turns stream normally.

```
1. If loader has active tools: inject tool definitions into LLM request
2. Call LLM (complete, not stream)
3. If response contains tool_calls:
   a. Persist assistant message (role="assistant", tool_calls array)
   b. For each call:
      - If NOT --allow-all: request human audit; on rejection,
        persist ToolResult { is_error: true, "rejected by user" } and skip
      - If approved (or --allow-all): resolve tool → enforce sandbox → execute
      - Persist tool result message (role="tool", tool_call_id)
   c. Append all results to context, call LLM again
4. Repeat until no tool_calls in response (max 10 iterations — guard against loops)
5. Stream final text response normally
```

## Human Audit

Without `--allow-all`, every tool call requires explicit user approval before
execution. The audit surface differs by command:

- **`ur chat`** — ftxui modal block shows tool name + formatted args; user
  presses `y` to approve or `n` to reject inline in the TUI
- **`ur run`** — prints tool name + args to stderr and reads a `y/n` line from
  stdin; non-interactive environments (piped stdin) auto-reject

`Tui` gains a new non-pure virtual method `request_tool_audit()` defaulting to
`return true` so existing `MockTui` stubs do not break.

Approval/rejection is recorded as a `"tool_audit"` message role in the session
for auditability. `--allow-all` bypasses the audit entirely. `build_window()`
excludes `"tool_audit"` and `"reason"` roles from context sent to the LLM.

## Sandbox Tier 1: Workspace Constraint

All file paths in tool arguments are validated inside each tool's `execute()`
by calling `Sandbox::validate()` before any I/O:

- Resolve to absolute path (`std::filesystem::weakly_canonical`)
- Reject any path outside `$root/workspace/`
- Return a `ToolResult { is_error: true }` with a clear message on violation

`allow_all` in `SandboxPolicy` disables path validation.

## Tool Manifest

Tool configuration lives in `$root/tools/tools.json` — written once by the
user, persistent across invocations:

```json
{
  "tools": [
    { "name": "read_file",  "enabled": true },
    { "name": "write_file", "enabled": true },
    { "name": "bash",       "enabled": true, "timeout": 10 },
    { "name": "web_search", "enabled": true, "max_results": 5 },
    { "name": "web_fetch",  "enabled": true },
    { "name": "read_image", "enabled": false }
  ]
}
```

The loader reads this at startup. Per-invocation filtering uses the
`--allow`/`--deny`/`--no-tools` flags — no `--tools` CLI flag is needed.

Tools with `requires_allow_all() == true` are silently removed from the active
list when `--allow-all` is not set, regardless of manifest settings.

## `Message` Struct Extension

The `Message` struct in `provider.hpp` gains optional fields for tool calling:

```cpp
struct ToolCallEntry {
    std::string id;
    std::string name;
    std::string arguments_json;
};

struct Message {
    std::string role;
    std::string content;
    // Phase 4 — populated only for tool-related turns:
    std::vector<ToolCallEntry> tool_calls;  // non-empty for assistant tool_calls turn
    std::string tool_call_id;              // non-empty for role="tool" result turn
};
```

Two constructors added to preserve existing `{"role", "content"}` call sites:
`Message(std::string role, std::string content)` and a default constructor.

## CMake

Tool sources are folded into `ur_lib` (no separate static lib). On Linux,
`target_link_libraries(ur_lib PUBLIC dl)` added. `dl_compat.hpp` provides a
portable shim over `dlopen`/`LoadLibrary`.

## Acceptance Criteria

- [ ] Tools in `$root/tools/` are discovered and registered at startup
- [ ] `$root/tools/tools.json` controls which tools are enabled and their settings
- [ ] LLM tool calls are routed through the executor and results returned to LLM
- [ ] Tier 1 sandbox blocks any path outside `$root/workspace/`
- [ ] `--allow-all` allows arbitrary paths and bypasses human audit
- [ ] Without `--allow-all`, tool calls are held for user approval before execution
- [ ] Rejected tool calls return an error result to the LLM without executing
- [ ] Invalid tool name in LLM response returns a clear error result (no crash)
- [ ] `web_search` is disabled with a warning when `UR_SEARCH_BASE_URL` is not set
- [ ] `read_image` is disabled when `ServerInfo::supports_vision` is false, even if `tools.json` enables it
- [ ] Tool-calling loop is capped at 10 iterations to prevent infinite loops
- [ ] `"tool_audit"` messages are persisted but excluded from LLM context window
