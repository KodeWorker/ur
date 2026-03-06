#include <gtest/gtest.h>

#include "memory/crypto.hpp"

namespace {

TEST(CryptoTest, LoadKeyReturnsEmptyWhenFileAbsent) {
  // TODO: call load_key() with a path that does not exist,
  //       assert the result is empty.
}

TEST(CryptoTest, LoadKeyReturnsContentsWhenFilePresent) {
  // TODO: write a known key to a temp file, call load_key(),
  //       assert the returned string matches the file contents.
}

TEST(CryptoTest, EncryptDecryptRoundTrip) {
  // TODO: generate or hardcode a 32-byte key string,
  //       encrypt a plaintext, decrypt the ciphertext,
  //       assert the result equals the original plaintext.
}

TEST(CryptoTest, DecryptFailsOnTamperedCiphertext) {
  // TODO: encrypt a plaintext, flip a byte in the ciphertext,
  //       assert that decrypt() throws std::runtime_error.
}

TEST(CryptoTest, EncryptProducesUniqueIVEachCall) {
  // TODO: encrypt the same plaintext twice with the same key,
  //       assert the two ciphertexts differ (different random IVs).
}

} // namespace
