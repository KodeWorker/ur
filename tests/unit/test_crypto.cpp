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

TEST_F(CryptoFileTest, LoadKeyThrowsWhenFileAbsent) {
  EXPECT_THROW(ur::load_key(key_path_), std::runtime_error);
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

TEST_F(CryptoFileTest, GenerateKeyCreatesFile) {
  ur::generate_key(key_path_);
  EXPECT_TRUE(fs::exists(key_path_));
}

TEST_F(CryptoFileTest, GenerateKeyIs32Bytes) {
  ur::generate_key(key_path_);
  EXPECT_EQ(fs::file_size(key_path_), 32u);
}

TEST_F(CryptoFileTest, GenerateKeyIsIdempotent) {
  ur::generate_key(key_path_);
  const std::string first = ur::load_key(key_path_);
  ur::generate_key(key_path_);  // second call — must not overwrite
  EXPECT_EQ(ur::load_key(key_path_), first);
}

}  // namespace
