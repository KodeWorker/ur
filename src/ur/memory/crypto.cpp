#include "crypto.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ur {

std::string load_key(const std::filesystem::path& key_path) {
  // open key_path in binary mode, read all bytes, return as string.
  // Throw std::runtime_error if the file cannot be opened.
  std::ifstream file(key_path, std::ios::binary);
  if (!file) {
    if (!std::filesystem::exists(key_path)) return {};
    throw std::runtime_error("load_key: failed to open file");
  }
  std::string key((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());
  return key;
}

std::string encrypt(const std::string& plaintext, const std::string& key) {
  // implement AES-256-GCM encryption using OpenSSL EVP API.
  //
  // Steps:
  //   1. Generate a 12-byte random IV with RAND_bytes().
  //   2. Create and initialise an EVP_CIPHER_CTX with EVP_aes_256_gcm().
  //   3. Set key (32 bytes from key) and IV.
  //   4. Encrypt plaintext → ciphertext.
  //   5. Finalise and retrieve the 16-byte auth tag via EVP_CIPHER_CTX_ctrl().
  //   6. Return: IV + ciphertext + tag.
  //   Throw std::runtime_error on any OpenSSL failure.

  if (key.size() != 32) {
    throw std::runtime_error("encrypt: key must be 32 bytes for AES-256-GCM");
  }

  std::vector<unsigned char> iv(12);
  if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
    throw std::runtime_error("encrypt: failed to generate IV");
  }
  auto ctx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) {
    throw std::runtime_error("encrypt: failed to create cipher context");
  }
  if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                         nullptr) != 1) {
    throw std::runtime_error("encrypt: failed to initialize encryption");
  }
  // Set key and IV
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
                          static_cast<int>(iv.size()), nullptr) != 1) {
    throw std::runtime_error("encrypt: failed to set IV length");
  }
  if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
                         reinterpret_cast<const unsigned char*>(key.data()),
                         iv.data()) != 1) {
    throw std::runtime_error("encrypt: failed to set key and IV");
  }
  // Provide the message to be encrypted
  std::vector<unsigned char> ciphertext(
      plaintext.size() + EVP_MAX_BLOCK_LENGTH);  // ciphertext can be up to
                                                 // plaintext size + tag size
  int len;
  if (EVP_EncryptUpdate(
          ctx.get(), ciphertext.data(), &len,
          reinterpret_cast<const unsigned char*>(plaintext.data()),
          static_cast<int>(plaintext.size())) != 1) {
    throw std::runtime_error("encrypt: failed to encrypt plaintext");
  }
  // Finalise encryption
  int ciphertext_len = len;
  if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
    throw std::runtime_error("encrypt: failed to finalize encryption");
  }
  ciphertext_len += len;
  // Get the tag
  std::vector<unsigned char> tag(16);
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
                          static_cast<int>(tag.size()), tag.data()) != 1) {
    throw std::runtime_error("encrypt: failed to get auth tag");
  }
  // Construct the final output: IV + ciphertext + tag
  std::string result;
  result.reserve(iv.size() + ciphertext_len + tag.size());
  result.append(reinterpret_cast<const char*>(iv.data()), iv.size());
  result.append(reinterpret_cast<const char*>(ciphertext.data()),
                ciphertext_len);
  result.append(reinterpret_cast<const char*>(tag.data()), tag.size());
  return result;
}

std::string decrypt(const std::string& ciphertext, const std::string& key) {
  // implement AES-256-GCM decryption using OpenSSL EVP API.
  //
  // Steps:
  //   1. Split ciphertext into IV (first 12 bytes), body, tag (last 16 bytes).
  //   2. Create and initialise an EVP_CIPHER_CTX with EVP_aes_256_gcm().
  //   3. Set key and IV.
  //   4. Decrypt body → plaintext.
  //   5. Set the expected auth tag via EVP_CIPHER_CTX_ctrl() and finalise.
  //      Throw std::runtime_error if authentication fails.
  //   6. Return plaintext.

  if (key.size() != 32) {
    throw std::runtime_error("decrypt: key must be 32 bytes for AES-256-GCM");
  }

  if (ciphertext.size() < 12 + 16) {
    throw std::runtime_error("decrypt: ciphertext too short");
  }
  const unsigned char* iv =
      reinterpret_cast<const unsigned char*>(ciphertext.data());
  const unsigned char* body =
      reinterpret_cast<const unsigned char*>(ciphertext.data() + 12);
  const unsigned char* tag = reinterpret_cast<const unsigned char*>(
      ciphertext.data() + ciphertext.size() - 16);
  size_t body_len = ciphertext.size() - 12 - 16;
  auto ctx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx) {
    throw std::runtime_error("decrypt: failed to create cipher context");
  }
  if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                         nullptr) != 1) {
    throw std::runtime_error("decrypt: failed to initialize decryption");
  }
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) !=
      1) {
    throw std::runtime_error("decrypt: failed to set IV length");
  }
  if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
                         reinterpret_cast<const unsigned char*>(key.data()),
                         iv) != 1) {
    throw std::runtime_error("decrypt: failed to set key and IV");
  }
  std::vector<unsigned char> plaintext(body_len);
  int len;
  if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, body,
                        static_cast<int>(body_len)) != 1) {
    throw std::runtime_error("decrypt: failed to decrypt ciphertext");
  }
  int plaintext_len = len;
  // Set expected tag value
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
                          const_cast<unsigned char*>(tag)) != 1) {
    throw std::runtime_error("decrypt: failed to set expected auth tag");
  }
  // Finalise decryption - returns 0 if authentication fails
  if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
    throw std::runtime_error("decrypt: authentication failed");
  }
  plaintext_len += len;
  return std::string(reinterpret_cast<const char*>(plaintext.data()),
                     plaintext_len);
}

}  // namespace ur
