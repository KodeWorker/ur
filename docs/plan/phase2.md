# Phase 2 — Single-Turn Inference

**Goal**: `ur run` sends a prompt to an OpenAI-compatible LLM server and prints the response. Session and messages are persisted. The LLM server is managed independently — `ur` connects to it over HTTP.

**Prerequisite**: Phase 1 complete.

## Deliverables

- `ur run <prompt> [--model=<name>] [--system-prompt=...] [--allow-all]`
- Response printed to stdout
- Session + messages written to database

## Source Files to Create

```
src/ur/llm/provider.hpp             Abstract LLM provider interface
src/ur/llm/http_provider.cpp/.hpp   OpenAI-compatible HTTP implementation
src/ur/llm/registry.cpp/.hpp        Map model string → provider instance
src/ur/agent/runner.cpp/.hpp        Single-turn request: build prompt, call LLM, return response
tests/unit/test_http_provider.cpp
tests/unit/test_runner.cpp
```

## Provider Interface

```cpp
// src/ur/llm/provider.hpp
struct Message { std::string role; std::string content; };

class Provider {
public:
    virtual ~Provider() = default;
    virtual std::string complete(const std::vector<Message>& messages) = 0;
};
```

## HTTP Provider

`HttpProvider` calls a remote OpenAI-compatible server (`POST /v1/chat/completions`).

Configuration (env vars or `.env`):
- `UR_LLM_BASE_URL` — server base URL (default: `http://localhost:8000`)
- `UR_LLM_API_KEY` — Bearer token (optional; leave empty for local servers)

The provider is stateless — no local model files or library linkage required.

## Runner Flow (`ur run`)

1. Parse args: extract prompt, system-prompt, model string
2. Create session row in DB (generate UUID, timestamp)
3. Build `messages` vector: optional system message + user message
4. Instantiate provider via registry
5. Call `provider.complete(messages)`
6. Insert user message and assistant message into DB (content encrypted if key loaded)
7. Print response to stdout

## Model Identifier Format

`--model=<name>` — passed directly to the server (e.g. `--model=mistral`, `--model=llama3`).

Default: omit `--model` and the server's default model is used.

## Acceptance Criteria

- [ ] `ur run "hello"` completes and prints a response
- [ ] Session and both messages appear in the database afterward
- [ ] Message content is encrypted at rest when `$root/keys/secret.key` is present
- [ ] `--system-prompt=file.txt` loads the file and prepends as a system message
- [ ] Unreachable server exits with a clear error message
- [ ] `UR_LLM_BASE_URL` overrides the default endpoint
