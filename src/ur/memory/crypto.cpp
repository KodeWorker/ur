#include "crypto.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace ur {

std::string load_key(const std::filesystem::path& key_path) {
  std::ifstream file(key_path, std::ios::binary);
  if (!file) {
    if (!std::filesystem::exists(key_path))
      throw std::runtime_error("load_key: key file not found — run 'ur init'");
    throw std::runtime_error("load_key: failed to open file");
  }
  std::string key((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());
  return key;
}

std::string random_bytes(size_t n) {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  const unsigned char pers[] = "ur";
  int rc = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                 pers, sizeof(pers) - 1);
  if (rc != 0) {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    throw std::runtime_error("random_bytes: failed to seed DRBG");
  }

  std::string result(n, '\0');
  rc = mbedtls_ctr_drbg_random(&ctr_drbg,
                               reinterpret_cast<unsigned char*>(&result[0]), n);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  if (rc != 0) {
    throw std::runtime_error("random_bytes: DRBG generation failed");
  }
  return result;
}

void generate_key(const std::filesystem::path& key_path) {
  if (std::filesystem::exists(key_path)) return;
  std::string key = random_bytes(32);
  std::ofstream f(key_path, std::ios::binary);
  if (!f) {
    throw std::runtime_error("generate_key: failed to open file for writing: " +
                             key_path.string());
  }
  f.write(key.data(), static_cast<std::streamsize>(key.size()));
  f.close();
  // Restrict to owner read/write only (POSIX). No-op on Windows.
  std::filesystem::permissions(
      key_path,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace);
}

std::string encrypt(const std::string& plaintext, const std::string& key) {
  if (key.size() != 32) {
    throw std::runtime_error("encrypt: key must be 32 bytes for AES-256-GCM");
  }

  std::string iv = random_bytes(12);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int rc = mbedtls_gcm_setkey(
      &gcm, MBEDTLS_CIPHER_ID_AES,
      reinterpret_cast<const unsigned char*>(key.data()), 256);
  if (rc != 0) {
    mbedtls_gcm_free(&gcm);
    throw std::runtime_error("encrypt: failed to set key");
  }

  std::string ciphertext(plaintext.size(), '\0');
  std::string tag(16, '\0');

  rc = mbedtls_gcm_crypt_and_tag(
      &gcm, MBEDTLS_GCM_ENCRYPT, plaintext.size(),
      reinterpret_cast<const unsigned char*>(iv.data()), iv.size(), nullptr, 0,
      reinterpret_cast<const unsigned char*>(plaintext.data()),
      reinterpret_cast<unsigned char*>(&ciphertext[0]), 16,
      reinterpret_cast<unsigned char*>(&tag[0]));

  mbedtls_gcm_free(&gcm);

  if (rc != 0) {
    throw std::runtime_error("encrypt: GCM encryption failed");
  }
  return iv + ciphertext + tag;
}

std::string decrypt(const std::string& ciphertext, const std::string& key) {
  if (key.size() != 32) {
    throw std::runtime_error("decrypt: key must be 32 bytes for AES-256-GCM");
  }
  if (ciphertext.size() < 12 + 16) {
    throw std::runtime_error("decrypt: ciphertext too short");
  }

  const size_t iv_len = 12;
  const size_t tag_len = 16;
  const size_t body_len = ciphertext.size() - iv_len - tag_len;

  const auto* iv = reinterpret_cast<const unsigned char*>(ciphertext.data());
  const auto* body =
      reinterpret_cast<const unsigned char*>(ciphertext.data() + iv_len);
  const auto* tag = reinterpret_cast<const unsigned char*>(ciphertext.data() +
                                                           iv_len + body_len);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int rc = mbedtls_gcm_setkey(
      &gcm, MBEDTLS_CIPHER_ID_AES,
      reinterpret_cast<const unsigned char*>(key.data()), 256);
  if (rc != 0) {
    mbedtls_gcm_free(&gcm);
    throw std::runtime_error("decrypt: failed to set key");
  }

  std::string plaintext(body_len, '\0');

  rc = mbedtls_gcm_auth_decrypt(
      &gcm, body_len, iv, iv_len, nullptr, 0, tag, tag_len, body,
      reinterpret_cast<unsigned char*>(&plaintext[0]));

  mbedtls_gcm_free(&gcm);

  if (rc != 0) {
    throw std::runtime_error("decrypt: authentication failed");
  }
  return plaintext;
}

}  // namespace ur
