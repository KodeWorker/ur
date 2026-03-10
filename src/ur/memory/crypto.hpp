#pragma once

#include <filesystem>
#include <string>

namespace ur {

// Reads $root/key/secret.key and returns its contents as a raw byte string.
// Throws std::runtime_error if the file does not exist or cannot be read.
std::string load_key(const std::filesystem::path& key_path);

// Generate a 32-byte key using OS entropy (mbedTLS CTR-DRBG) and write it to
// key_path. No-op if key_path already exists — never overwrites an existing
// key. Throws std::runtime_error on failure.
void generate_key(const std::filesystem::path& key_path);

// Fill n bytes with cryptographically random data (mbedTLS CTR-DRBG).
// Throws std::runtime_error on failure.
std::string random_bytes(size_t n);

// AES-256-GCM encrypt plaintext with key.
// Output format: [ 12-byte random IV ][ ciphertext ][ 16-byte auth tag ]
// Throws std::runtime_error on failure.
std::string encrypt(const std::string& plaintext, const std::string& key);

// AES-256-GCM decrypt ciphertext with key.
// Expects the format produced by encrypt().
// Throws std::runtime_error on failure or authentication mismatch.
std::string decrypt(const std::string& ciphertext, const std::string& key);

}  // namespace ur
