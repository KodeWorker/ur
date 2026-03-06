#pragma once

#include <filesystem>
#include <string>

namespace ur {

// Reads $root/keys/secret.key and returns its contents as a raw byte string.
// Returns an empty string if the file does not exist (encryption disabled).
// Throws std::runtime_error if the file exists but cannot be read.
std::string load_key(const std::filesystem::path &key_path);

// AES-256-GCM encrypt plaintext with key.
// Output format: [ 12-byte random IV ][ ciphertext ][ 16-byte auth tag ]
// Throws std::runtime_error on failure.
std::string encrypt(const std::string &plaintext, const std::string &key);

// AES-256-GCM decrypt ciphertext with key.
// Expects the format produced by encrypt().
// Throws std::runtime_error on failure or authentication mismatch.
std::string decrypt(const std::string &ciphertext, const std::string &key);

} // namespace ur
