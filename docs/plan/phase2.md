# Phase 2 — Single-Turn Inference

**Goal**: `ur run` sends a prompt to llama.cpp and prints the response. Session and messages are persisted.

**Prerequisite**: Phase 1 complete.

## Deliverables

- `ur run <prompt> [--model=llama.cpp/<name>] [--system-prompt=...] [--allow-all]`
- Response printed to stdout
- Session + messages written to database

## Source Files to Create

```
src/ur/llm/provider.hpp             Abstract LLM provider interface
src/ur/llm/llama_provider.cpp/.hpp  llama.cpp implementation
src/ur/llm/registry.cpp/.hpp        Map "provider/model" string → provider instance
src/ur/agent/runner.cpp/.hpp        Single-turn request: build prompt, call LLM, return response
tests/unit/test_llama_provider.cpp
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

## llama.cpp Integration

- Link against llama.cpp as a CMake subdirectory (`third_party/llama.cpp`)
- Model file resolved from `--model=llama.cpp/<filename>` → `$root/keys/<filename>`
- Load model once at provider construction; reuse across calls

## Runner Flow (`ur run`)

1. Parse args: extract prompt, system-prompt, model string
2. Create session row in DB (generate UUID, timestamp)
3. Build `messages` vector: optional system message + user message
4. Instantiate provider via registry
5. Call `provider.complete(messages)`
6. Insert user message and assistant message into DB
7. Print response to stdout

## Model Identifier Format

`--model=<provider>/<name>` e.g. `--model=llama.cpp/mistral-7b-q4.gguf`

Default: `llama.cpp/default` — resolved to the first GGUF file found in `$root/keys/`.

## Acceptance Criteria

- [ ] `ur run "hello"` completes and prints a response
- [ ] Session and both messages appear in the database afterward
- [ ] `--system-prompt=file.txt` loads the file and prepends as a system message
- [ ] Unknown `--model` value exits with a clear error
