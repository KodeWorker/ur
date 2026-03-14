# Phase 6 — Docker Sandbox and Streaming TUI

**Goal**: sandbox tier 2 runs tool execution inside a Docker container. TUI is polished with streaming output.

**Prerequisite**: Phase 5 complete.

## Deliverables

- Sandbox tier 2: tool execution inside Docker container
- TUI refinements (streaming output, status line)

## Source Files to Create

```
src/ur/tools/docker_runner.cpp/.hpp     Tier 2 sandbox: spawn container, execute tool, collect output
src/ur/cli/tui_stream.cpp/.hpp          Streaming token output to terminal
src/ur/cli/md_render.cpp/.hpp           md4c AST → ftxui Element renderer
tests/unit/test_docker_runner.cpp
tests/unit/test_md_render.cpp
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

## TUI Refinements

- Stream tokens to terminal as they arrive (provider must support streaming)
- Status line showing: current model, session ID, turn count
- Graceful Ctrl-C handling (finish current turn before exiting)
- Markdown rendering via **md4c** (CommonMark parser, C library, ~50 KB,
  fetched via FetchContent). The renderer walks the md4c AST and emits ftxui
  elements: bold, italic, inline code, fenced code blocks, headers, bullet and
  ordered lists, blockquotes. Applied to assistant message content in
  `print_response()`.

## Acceptance Criteria

- [ ] Tool calls execute inside a Docker container under tier 2
- [ ] Container is removed after each tool call (`--rm`)
- [ ] Tokens stream to terminal in real time during chat
- [ ] `ur chat` handles Ctrl-C cleanly without data loss
- [ ] Assistant responses render CommonMark markdown (bold, italic, inline code,
      code blocks, headers, lists, blockquotes)
- [ ] Plain-text fallback when md4c parsing fails
