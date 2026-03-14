# Phase 7 — Code Agent (Python Tool Execution)

**Goal**: agents can express complex multi-step tool logic as generated Python
code, executed inside the Docker sandbox. Replaces the one-call-at-a-time JSON
tool-calling loop with a single LLM turn that produces a Python script
orchestrating arbitrarily many tool calls with real control flow.

**Prerequisite**: Phase 6 complete (Docker sandbox tier 2 required for safe
code execution).

## Motivation

The JSON tool-calling loop (Phase 4) requires one LLM roundtrip per tool call.
For complex workflows — iterate over files, branch on results, retry on error —
this produces many turns and high latency. The code-agent pattern lets the LLM
write a short Python script that encodes the entire logic flow and executes it
in one shot.

```
[agent]  →  [generated Python script]  →  [Python tool stubs]  →  results
                 (single LLM turn)            (inside container)
```

This is the same pattern used by HuggingFace `smolagents` and OpenAI code
interpreter.

## Deliverables

- `code_agent` mode: LLM generates Python instead of JSON tool calls
- Python tool stubs auto-generated from the `Tool` ABC registry — same tools,
  same sandbox policy, new calling convention
- Code executed inside Docker container (tier 2 sandbox); no interpreter on host
- Execution output (stdout + stderr) returned to the LLM as the tool result
- `ur run --code-agent` / `ur chat --code-agent` flags to opt in
- Human audit: show generated script before execution (same `y/n` flow as Phase 4)

## Source Files to Create

```
src/ur/agent/code_agent.cpp/.hpp        Code agent mode: prompt builder + execution loop
src/ur/tools/stub_gen.cpp/.hpp          Generate Python stub module from Tool registry
src/ur/tools/code_runner.cpp/.hpp       Write script + stubs to temp dir, exec in container
tests/unit/test_stub_gen.cpp
tests/unit/test_code_runner.cpp
```

## Architecture

### Prompt strategy

The system prompt is augmented with:
1. A description of available tools (same as Phase 4, but formatted as Python
   function signatures with docstrings instead of JSON schemas)
2. An instruction to write a self-contained Python script that calls those
   functions and prints its final result

Example tool stub injected into the container:

```python
def read_file(path: str) -> str:
    """Read a file from the workspace. path must be inside /workspace."""
    ...

def web_fetch(url: str) -> str:
    """Fetch the content of a URL and return it as plain text."""
    ...
```

The LLM generates code like:

```python
content = read_file("notes.txt")
if "TODO" in content:
    page = web_fetch("https://example.com/docs")
    write_file("summary.txt", page[:500])
print("done")
```

### Execution flow

```
1. Build system prompt with Python stub signatures
2. Call LLM → response is a Python script (fenced code block)
3. Extract script from response
4. If NOT --allow-all: show script to user for approval (y/n)
5. Write script + generated stubs module to a temp dir
6. docker run --rm -v $root/workspace:/workspace -v $tmpdir:/agent
       <image> python /agent/script.py
7. Capture stdout/stderr as the result
8. Append result to context, call LLM for final natural-language response
9. Repeat from step 2 if LLM emits another script block
```

### Stub generation (`stub_gen`)

`StubGen` iterates the registered `Tool` instances and emits a Python module
(`tools.py`) where each function:
- Has the correct signature derived from `input_schema_json()`
- Calls a host-side dispatcher via a Unix socket or a small HTTP server running
  in the container sidecar — keeping actual tool logic on the host, enforced
  by the same `SandboxPolicy` as Phase 4

This means the Python code runs inside Docker but tool side-effects (file I/O,
web fetch) are still routed through the host sandbox layer — no tool logic
needs to be ported to Python.

### Fallback

If the LLM response contains no fenced Python block, `code_agent` falls back
to the standard JSON tool-calling loop (Phase 4 behaviour). This handles models
that do not support code generation well.

## Tool manifest

`tools.json` gains a `"modes"` field per tool:

```json
{ "name": "read_file", "enabled": true, "modes": ["json", "code"] }
```

`"modes"` defaults to `["json", "code"]` if absent. Setting `"modes":
["json"]` excludes a tool from the Python stub module while keeping it
available for standard tool calls.

## Acceptance Criteria

- [ ] `ur run --code-agent <prompt>` generates and executes a Python script
- [ ] `ur chat --code-agent` enables code-agent mode for the session
- [ ] Python stubs are auto-generated from the live Tool registry
- [ ] Script executes inside the Docker container (tier 2 sandbox)
- [ ] Tool calls from Python are routed through the host sandbox layer
- [ ] Without `--allow-all`, generated script is shown to user before execution
- [ ] Rejected scripts return an error result to the LLM without executing
- [ ] Fallback to JSON tool-calling loop when no Python block is produced
- [ ] `stdout` + `stderr` from script execution are captured and returned to LLM
- [ ] `"modes"` field in `tools.json` controls per-tool availability in code mode
