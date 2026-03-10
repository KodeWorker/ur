# Phase 2.1 — Replace OpenSSL with mbedTLS (Standalone Build)

**Goal**: eliminate the OpenSSL system dependency so that `ur` builds from source with no
external libraries required. Replace OpenSSL with mbedTLS (pulled via FetchContent). Add
automatic key generation in `ur init` so users no longer run `openssl rand` manually.

**Prerequisite**: Phase 2 complete.

## Motivation

OpenSSL is the only remaining system dependency. All other dependencies (SQLite, cpp-httplib,
nlohmann/json, GoogleTest) are either bundled or fetched automatically by CMake. mbedTLS has a
native `CMakeLists.txt` and is FetchContent-compatible, making a fully standalone build possible
without bundling a large C library manually.

## Deliverables

- OpenSSL removed from all source files and CMake
- mbedTLS fetched via FetchContent, linked as `MbedTLS::mbedcrypto`
- `crypto.cpp` rewritten with mbedTLS GCM + CTR-DRBG
- `generate_key()` added to the crypto API — called automatically by `ur init`
- `random_bytes()` added as a shared RNG helper, replacing `RAND_bytes()` in `runner.cpp`
- README updated: OpenSSL removed from dependencies section; key generation documented as
  automatic

## Source Files to Modify

```
CMakeLists.txt                      Add FetchContent for mbedTLS; remove OpenSSL comment
src/ur/CMakeLists.txt               Remove find_package(OpenSSL); link MbedTLS::mbedcrypto
src/ur/memory/crypto.hpp            Add generate_key() and random_bytes() declarations
src/ur/memory/crypto.cpp            Rewrite encrypt/decrypt with mbedTLS GCM; implement
                                    generate_key() and random_bytes() with CTR-DRBG
src/ur/agent/runner.cpp             Replace RAND_bytes() with random_bytes()
src/ur/cli/command.cpp              cmd_init: call generate_key() when key file absent
tests/unit/test_crypto.cpp          Add GenerateKeyCreatesFile, GenerateKeyIs32Bytes,
                                    GenerateKeyIsIdempotent (does not overwrite)
README.md                           Remove OpenSSL from Dependencies; update Security section
```

## CMake Changes

### Root `CMakeLists.txt`

```cmake
# mbedTLS — crypto only (no TLS stack needed)
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG        v3.6.2)
set(ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)
```

Only `MbedTLS::mbedcrypto` is linked — the TLS and X.509 modules are not needed.

### `src/ur/CMakeLists.txt`

```cmake
# Before (remove):
find_package(OpenSSL REQUIRED)
target_link_libraries(ur_lib PUBLIC ... OpenSSL::Crypto ...)

# After:
target_link_libraries(ur_lib PUBLIC sqlite3 MbedTLS::mbedcrypto httplib::httplib
                                    nlohmann_json::nlohmann_json)
```

cpp-httplib will compile in HTTP-only mode once OpenSSL is absent. This is correct — `ur`
targets local HTTP servers; HTTPS is delegated to a reverse proxy.

## Updated `crypto.hpp` Interface

```cpp
namespace ur {

// Returns empty string if key_path does not exist (encryption disabled).
// Throws std::runtime_error if the file exists but cannot be read.
std::string load_key(const std::filesystem::path& key_path);

// Generate a 32-byte key using OS entropy (mbedTLS CTR-DRBG) and write it to
// key_path. No-op if key_path already exists (never overwrites).
// Throws std::runtime_error on failure.
void generate_key(const std::filesystem::path& key_path);

// Fill buf with n cryptographically random bytes (mbedTLS CTR-DRBG).
// Throws std::runtime_error on failure.
std::string random_bytes(size_t n);

// AES-256-GCM encrypt / decrypt. Wire format unchanged:
// [ 12-byte random IV ][ ciphertext ][ 16-byte auth tag ]
std::string encrypt(const std::string& plaintext, const std::string& key);
std::string decrypt(const std::string& ciphertext, const std::string& key);

}  // namespace ur
```

`random_bytes()` is the single RNG entry point for the rest of the codebase. `runner.cpp`
calls `random_bytes(16)` for session/message IDs; `encrypt()` calls it internally for IV
generation.

## `crypto.cpp` Implementation Notes

### CTR-DRBG setup (shared per call, seeded from entropy)

```cpp
// mbedtls headers needed:
#include <mbedtls/gcm.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
```

`random_bytes(n)`:
1. Initialise `mbedtls_entropy_context` and `mbedtls_ctr_drbg_context`
2. `mbedtls_ctr_drbg_seed()` with a personalisation string (`"ur"`)
3. `mbedtls_ctr_drbg_random()` to fill the buffer
4. Free both contexts; return bytes as `std::string`

`encrypt(plaintext, key)`:
1. Generate 12-byte IV via `random_bytes(12)`
2. `mbedtls_gcm_init()` → `mbedtls_gcm_setkey(MBEDTLS_CIPHER_ID_AES, key, 256)`
3. `mbedtls_gcm_crypt_and_tag(MBEDTLS_GCM_ENCRYPT, ...)` — produces ciphertext + 16-byte tag
4. Return `IV + ciphertext + tag`; free GCM context

`decrypt(ciphertext, key)`:
1. Split: IV (first 12 bytes), body, tag (last 16 bytes)
2. `mbedtls_gcm_init()` → `mbedtls_gcm_setkey(...)`
3. `mbedtls_gcm_auth_decrypt(...)` — verifies tag and decrypts
4. Return plaintext; free GCM context

Wire format (`IV + ciphertext + tag`) is identical to the Phase 1/2 OpenSSL implementation,
so no database migration is required for databases that were not yet encrypted in production.

`generate_key(key_path)`:
1. Return immediately if `key_path` exists (idempotent — never overwrites an existing key)
2. Call `random_bytes(32)` to generate the key material
3. Write raw bytes to `key_path` in binary mode
4. On POSIX: `chmod(key_path, 0600)` via `std::filesystem` permissions

## `cmd_init` Changes

```
ur init now:
  1. init_workspace(ctx.paths)          — unchanged
  2. ctx.db.init_schema()               — unchanged
  3. generate_key(ctx.paths.keys / "secret.key")   — NEW: no-op if key exists
  4. Print: "Workspace initialized at <root>"
  5. Print: "Encryption key generated."   (only if key was newly created)
```

`make_context()` still reads the key after init and validates it. If `ur init` is re-run,
`generate_key()` skips generation and the existing key is preserved.

## README Changes

### Dependencies section

Remove:
```
- OpenSSL 3+
  - Linux: `libssl-dev` ...
  - macOS: `brew install openssl`
  - Windows: `vcpkg install openssl` — pass `-DCMAKE_TOOLCHAIN_FILE=...`
```

All dependencies are now handled by CMake automatically.

### Security — Database encryption section

Replace the manual key generation block:
```shell
# Before (remove — no longer required):
openssl rand -out ~/.local/share/ur/key/secret.key 32
chmod 600 ~/.local/share/ur/key/secret.key
```

With:
```
`ur init` generates a 256-bit encryption key automatically. No manual step required.
```

## Acceptance Criteria

- [ ] `cmake -B build && cmake --build build` succeeds with no system OpenSSL installed
- [ ] `ur init` creates `$root/key/secret.key` (32 bytes, mode 0600 on POSIX)
- [ ] Re-running `ur init` does not overwrite an existing key
- [ ] `ur run` encrypts and decrypts message content correctly with the auto-generated key
- [ ] All existing unit tests pass unchanged
- [ ] `GenerateKeyCreatesFile` — key file exists after `generate_key()`
- [ ] `GenerateKeyIs32Bytes` — key file is exactly 32 bytes
- [ ] `GenerateKeyIsIdempotent` — second call leaves the file unchanged
- [ ] No `#include <openssl/...>` remains in any source file
