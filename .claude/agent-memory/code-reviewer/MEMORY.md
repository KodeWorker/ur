# Code Reviewer Agent Memory

## Project: ur (C++17 CLI agent sandbox)

### Phase 1 Status
- Phase 1 declared complete on branch `impl/phase-1-labor`
- All 14 unit tests passing (test_workspace, test_database, test_crypto)
- Files: workspace.cpp, database.cpp, crypto.cpp, command.cpp, context.cpp, logger.cpp

### Phase 2 Status
- Branch: `impl/phase-2-labor`
- New files: http_provider, runner, env, logger (rewritten), provider interface
- Tests: test_http_provider (loopback server fixture), test_runner (MockProvider)
- Dependencies added: cpp-httplib v0.18.1, nlohmann/json v3.11.3 via FetchContent
- Database handle now uses unique_ptr<sqlite3, decltype(&sqlite3_close)> (RAII improvement)
- Code review #1: 9 issues filed under milestone "Phase 2 - Code Review #1" (#170-#178)

### Phase 3 Status
- Branch: `impl/phase-3-labor`
- 58 tests passing (added: test_chat, test_persona_updater, expanded test_database)
- New files: chat.cpp/hpp, persona_updater.cpp/hpp, tui.cpp/hpp (ftxui-based)
- Dependencies added: ftxui via FetchContent
- Features: ur chat (streaming, slash commands, tab autocomplete), ur history (id prefix), ur persona
- TUI: pimpl pattern (FtxuiTui::Impl), ScrollerBase for mouse/keyboard scrolling
- Streaming: shared_ptr<string> buffers for live-updating Renderer components
- Phase 3.1: --system-prompt, --allow/--deny/--no-tools/--allow-all flags on ur run
- Key issues found: data race on system_prompt (UI thread vs chat thread), LIKE wildcard injection, non-transactional chat DB writes

### Observed Patterns
- Database uses unique_ptr with sqlite3_close deleter (improved from Phase 1 raw pointer)
- Crypto uses raw `EVP_CIPHER_CTX*` with manual free (no RAII wrapper)
- Test fixtures use `getpid()`-isolated temp paths in /tmp
- Pre-commit: clang-format, cppcheck, cpplint all configured with `exclude: ^third_party/`
- CMake: `ur_lib` STATIC library links all sources except main.cpp; tests link against ur_lib
- `cppcheck-suppress` inline comments used for false positives (e.g., unusedStructMember)
- HttpProvider creates a new httplib::Client per call (no connection reuse)
- .env loading happens before make_context() in main()
- Transaction pattern: begin/try{writes}/commit/catch{rollback;throw}
- Network I/O done outside DB transactions (correct design)

### Recurring Issue Types
- Environment variable name inconsistencies (UR_LLM_MODEL_NAME vs UR_LLM_MODEL)
- Missing input validation on external data (JSON parse, URL format, crypto key sizes)
- std::stoi on env vars without error handling (recurring across phases)
- No timeouts on HTTP clients
- RAII not consistently used (sqlite3_stmt still manual finalize, OpenSSL contexts)
- Thread safety: data races on shared std::string between UI and worker threads
- Duplicate utility code (generate_id in runner.cpp and chat.cpp)
- LIKE queries without escaping wildcards (%, _)

### Conventions
- Namespace: `ur`
- Error handling: throw `std::runtime_error` with descriptive prefix (e.g., "Database::open: ...")
- Lazy initialization: Database and Logger defer file opens until first use
- Context struct: aggregate initialization, no I/O at construction
- Header guards: `#pragma once`
- Test naming: `TEST_F(FixtureTest, DescriptiveTestName)`

### GitHub Labels Available
- Severity: `[High]`, `[Medium]`, `[Low]` (no `[Critical]` or `[Nit]` labels yet)
- Category: `security`, `performance`, `design`, `testing`, `bug`, `architecture`, `code-quality`
