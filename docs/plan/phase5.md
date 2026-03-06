# Phase 5 — Docker Sandbox and Extended Providers

**Goal**: sandbox tier 2 runs tool execution inside a Docker container. Additional LLM providers are supported.

**Prerequisite**: Phase 4 complete.

## Deliverables

- Sandbox tier 2: tool execution inside Docker container
- At least one additional LLM provider (e.g. OpenAI-compatible API)
- TUI refinements (streaming output, status line)

## Source Files to Create

```
src/ur/tools/docker_runner.cpp/.hpp     Tier 2 sandbox: spawn container, execute tool, collect output
src/ur/llm/openai_provider.cpp/.hpp     OpenAI-compatible HTTP provider
src/ur/cli/tui_stream.cpp/.hpp          Streaming token output to terminal
tests/unit/test_docker_runner.cpp
tests/unit/test_openai_provider.cpp
```

## Sandbox Tier 2: Docker Runner

Tool execution flow:
1. Serialize tool name + args to a temp file
2. `docker run --rm -v $root/workspace:/workspace <image> ur-tool-runner <args>`
3. Capture stdout as the tool result
4. Container is ephemeral — no state persists between calls

Configuration (via `.env` or env vars):
- `UR_DOCKER_IMAGE` — container image to use
- `UR_DOCKER_EXTRA_ARGS` — additional `docker run` flags (e.g. network, memory limits)

Sandbox tier is selected automatically: if Docker is available and `--allow-all` is not set, tier 2 is preferred over tier 1.

## Additional LLM Provider: OpenAI-compatible

```cpp
class OpenAIProvider : public Provider {
    // POST /v1/chat/completions with Bearer token from $root/keys/
    // Reads UR_OPENAI_API_KEY or equivalent from environment
};
```

Model string format: `openai/<model-name>` e.g. `--model=openai/gpt-4o`

## TUI Refinements

- Stream tokens to terminal as they arrive (provider must support streaming)
- Status line showing: current model, session ID, turn count
- Graceful Ctrl-C handling (finish current turn before exiting)

## Acceptance Criteria

- [ ] Tool calls execute inside a Docker container under tier 2
- [ ] Container is removed after each tool call (`--rm`)
- [ ] `--model=openai/gpt-4o` routes through OpenAI-compatible provider
- [ ] Tokens stream to terminal in real time during chat
- [ ] `ur chat` handles Ctrl-C cleanly without data loss
