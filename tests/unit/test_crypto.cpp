#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "memory/crypto.hpp"

namespace {

namespace fs = std::filesystem;

// A valid 32-byte AES-256 key for testing.
const std::string kKey(32, 'k');

class CryptoFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    key_path_ = fs::temp_directory_path() /
                ("ur_test_key_" + std::to_string(::getpid()));
  }

  void TearDown() override { fs::remove(key_path_); }

  fs::path key_path_;
};

TEST_F(CryptoFileTest, LoadKeyReturnsEmptyWhenFileAbsent) {
  EXPECT_TRUE(ur::load_key(key_path_).empty());
}

TEST_F(CryptoFileTest, LoadKeyReturnsContentsWhenFilePresent) {
  std::ofstream(key_path_, std::ios::binary) << kKey;
  EXPECT_EQ(ur::load_key(key_path_), kKey);
}

TEST(CryptoTest, EncryptDecryptRoundTrip) {
  const std::string plaintext = "hello, ur!";
  const std::string ciphertext = ur::encrypt(plaintext, kKey);
  EXPECT_EQ(ur::decrypt(ciphertext, kKey), plaintext);
}

TEST(CryptoTest, DecryptFailsOnTamperedCiphertext) {
  std::string ciphertext = ur::encrypt("hello", kKey);
  // Flip a byte in the ciphertext body (past the 12-byte IV).
  ciphertext[15] ^= 0xFF;
  EXPECT_THROW(ur::decrypt(ciphertext, kKey), std::runtime_error);
}

TEST(CryptoTest, EncryptProducesUniqueIVEachCall) {
  const std::string plaintext = "same plaintext";
  const std::string c1 = ur::encrypt(plaintext, kKey);
  const std::string c2 = ur::encrypt(plaintext, kKey);
  EXPECT_NE(c1, c2);
}

}  // namespace
