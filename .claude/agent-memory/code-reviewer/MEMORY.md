# Code Reviewer Agent Memory

## Project: ur (C++17 CLI agent sandbox)

### Phase 1 Status
- Phase 1 declared complete on branch `impl/phase-1-labor`
- All 14 unit tests passing (test_workspace, test_database, test_crypto)
- Files: workspace.cpp, database.cpp, crypto.cpp, command.cpp, context.cpp, logger.cpp

### Observed Patterns
- Raw pointer management: `Database` uses raw `sqlite3*` with manual close in destructor
- Crypto uses raw `EVP_CIPHER_CTX*` with manual free (no RAII wrapper)
- Test fixtures use `getpid()`-isolated temp paths in /tmp
- Pre-commit: clang-format, cppcheck, cpplint all configured with `exclude: ^third_party/`
- CMake: `ur_lib` STATIC library links all sources except main.cpp; tests link against ur_lib
- `cppcheck-suppress` inline comments used for false positives (e.g., unusedStructMember)

### Recurring Issue Types
- Move semantics with raw resources (Database default move = double-free risk)
- Missing input validation on crypto key sizes
- RAII not used for OpenSSL contexts (manual free before every throw)

### Conventions
- Namespace: `ur`
- Error handling: throw `std::runtime_error` with descriptive prefix (e.g., "Database::open: ...")
- Lazy initialization: Database and Logger defer file opens until first use
- Context struct: aggregate initialization, no I/O at construction
- Header guards: `#pragma once`
- Test naming: `TEST_F(FixtureTest, DescriptiveTestName)`
