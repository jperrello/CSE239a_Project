#ifndef IMPROVED_CRYPTO_HPP
#define IMPROVED_CRYPTO_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/err.h>
#include <cstring>

constexpr int AES_KEY_SIZE = 32;    // 256 bits
constexpr int AES_BLOCK_SIZE = 16;  // Block size for AES
constexpr int HMAC_SIZE = 32;       // SHA256 output size

// A simple KeyManager for demonstration.
// In a production system, keys would be exchanged/rotated securely.
class KeyManager {
public:
    static const std::string& getKey() {
        static std::string key = initializeKey();
        return key;
    }
private:
    static std::string initializeKey() {
        unsigned char buf[AES_KEY_SIZE];
        if (RAND_bytes(buf, AES_KEY_SIZE) != 1)
            throw std::runtime_error("RAND_bytes failed in KeyManager");
        return std::string(reinterpret_cast<char*>(buf), AES_KEY_SIZE);
    }
};

// secure_encrypt_string:
// Encrypts a plaintext string using AES-256-CBC with a random IV and computes an HMAC for integrity.
// Output format: [IV (16 bytes)] || [ciphertext] || [HMAC (32 bytes)].
inline std::string secure_encrypt_string(const std::string& plaintext) {
    const std::string& key = KeyManager::getKey();
    unsigned char iv[AES_BLOCK_SIZE];
    if (RAND_bytes(iv, AES_BLOCK_SIZE) != 1)
        throw std::runtime_error("RAND_bytes failed in secure_encrypt_string (IV generation)");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed in secure_encrypt_string");

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, reinterpret_cast<const unsigned char*>(key.data()), iv) != 1)
        throw std::runtime_error("EVP_EncryptInit_ex failed in secure_encrypt_string");

    std::vector<unsigned char> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()) != 1)
        throw std::runtime_error("EVP_EncryptUpdate failed in secure_encrypt_string");
    int ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1)
        throw std::runtime_error("EVP_EncryptFinal_ex failed in secure_encrypt_string");
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    // Build output: IV || ciphertext
    std::string output(reinterpret_cast<char*>(iv), AES_BLOCK_SIZE);
    output.append(reinterpret_cast<char*>(ciphertext.data()), ciphertext_len);

    // Compute HMAC over (IV || ciphertext)
    unsigned char hmac[HMAC_SIZE];
    if (!HMAC(EVP_sha256(), key.data(), key.size(),
              reinterpret_cast<const unsigned char*>(output.data()), output.size(),
              hmac, NULL))
        throw std::runtime_error("HMAC computation failed in secure_encrypt_string");
    output.append(reinterpret_cast<char*>(hmac), HMAC_SIZE);

    return output;
}

// secure_decrypt_string:
// Reverses the encryption process: verifies the HMAC and decrypts the ciphertext.
// Throws an exception if HMAC verification fails.
inline std::string secure_decrypt_string(const std::string& input) {
    const std::string& key = KeyManager::getKey();
    if (input.size() < AES_BLOCK_SIZE + HMAC_SIZE)
        throw std::runtime_error("Input too short in secure_decrypt_string");

    size_t iv_offset = 0;
    size_t ciphertext_offset = AES_BLOCK_SIZE;
    size_t hmac_offset = input.size() - HMAC_SIZE;
    size_t ciphertext_len = hmac_offset - AES_BLOCK_SIZE;

    // Verify HMAC
    std::string data_to_auth = input.substr(0, hmac_offset);
    unsigned char computed_hmac[HMAC_SIZE];
    if (!HMAC(EVP_sha256(), key.data(), key.size(),
              reinterpret_cast<const unsigned char*>(data_to_auth.data()), data_to_auth.size(),
              computed_hmac, NULL))
        throw std::runtime_error("HMAC computation failed in secure_decrypt_string");
    if (CRYPTO_memcmp(computed_hmac, reinterpret_cast<const unsigned char*>(input.data() + hmac_offset), HMAC_SIZE) != 0)
        throw std::runtime_error("HMAC verification failed in secure_decrypt_string");

    const unsigned char* iv = reinterpret_cast<const unsigned char*>(input.data());
    const unsigned char* ciphertext = reinterpret_cast<const unsigned char*>(input.data() + ciphertext_offset);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed in secure_decrypt_string");
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, reinterpret_cast<const unsigned char*>(key.data()), iv) != 1)
        throw std::runtime_error("EVP_DecryptInit_ex failed in secure_decrypt_string");

    std::vector<unsigned char> plaintext(ciphertext_len + AES_BLOCK_SIZE);
    int len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext, ciphertext_len) != 1)
        throw std::runtime_error("EVP_DecryptUpdate failed in secure_decrypt_string");
    int plaintext_len = len;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1)
        throw std::runtime_error("EVP_DecryptFinal_ex failed in secure_decrypt_string");
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);
}

#endif 
