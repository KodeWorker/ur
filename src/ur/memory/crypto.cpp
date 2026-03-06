#include "crypto.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <fstream>
#include <stdexcept>
#include <string>

namespace ur {

std::string load_key(const std::filesystem::path& key_path) {
  if (!std::filesystem::exists(key_path)) return {};

  // TODO: open key_path in binary mode, read all bytes, return as string.
  //       Throw std::runtime_error if the file cannot be opened.
  throw std::runtime_error("load_key: not implemented");
}

std::string encrypt(const std::string& plaintext, const std::string& key) {
  // TODO: implement AES-256-GCM encryption using OpenSSL EVP API.
  //
  // Steps:
  //   1. Generate a 12-byte random IV with RAND_bytes().
  //   2. Create and initialise an EVP_CIPHER_CTX with EVP_aes_256_gcm().
  //   3. Set key (32 bytes from key) and IV.
  //   4. Encrypt plaintext → ciphertext.
  //   5. Finalise and retrieve the 16-byte auth tag via EVP_CIPHER_CTX_ctrl().
  //   6. Return: IV + ciphertext + tag.
  //   Throw std::runtime_error on any OpenSSL failure.
  (void)plaintext;
  (void)key;
  throw std::runtime_error("encrypt: not implemented");
}

std::string decrypt(const std::string& ciphertext, const std::string& key) {
  // TODO: implement AES-256-GCM decryption using OpenSSL EVP API.
  //
  // Steps:
  //   1. Split ciphertext into IV (first 12 bytes), body, tag (last 16 bytes).
  //   2. Create and initialise an EVP_CIPHER_CTX with EVP_aes_256_gcm().
  //   3. Set key and IV.
  //   4. Decrypt body → plaintext.
  //   5. Set the expected auth tag via EVP_CIPHER_CTX_ctrl() and finalise.
  //      Throw std::runtime_error if authentication fails.
  //   6. Return plaintext.
  (void)ciphertext;
  (void)key;
  throw std::runtime_error("decrypt: not implemented");
}

}  // namespace ur
