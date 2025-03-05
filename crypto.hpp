#ifndef CRYPTO_HPP
#define CRYPTO_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <cstring>

// constants for AES-GCM mode.
constexpr int AES_KEY_SIZE = 32;         // 256-bit key
constexpr int AES_GCM_IV_SIZE = 12;        // Recommended size for GCM (12 bytes)
constexpr int AES_GCM_TAG_SIZE = 16;       // Authentication tag size for GCM

// KeyManager:
// Generates a static random key for demonstration

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
        // Using std::string to store binary key data.
        return std::string(reinterpret_cast<char*>(buf), AES_KEY_SIZE);
    }
};

// secure_encrypt_string:
// Encrypts a plaintext string using AES-256-GCM which provides both confidentiality and integrity.
// Output format: [IV (12 bytes)] || [ciphertext] || [tag (16 bytes)]
inline std::string secure_encrypt_string(const std::string& plaintext) {
    const std::string& key = KeyManager::getKey();
    unsigned char iv[AES_GCM_IV_SIZE];
    if (RAND_bytes(iv, AES_GCM_IV_SIZE) != 1)
        throw std::runtime_error("RAND_bytes failed in secure_encrypt_string (IV generation)");

    // Create and initialize the context.
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("EVP_CIPHER_CTX_new failed in secure_encrypt_string");

    // Initialize encryption context with AES-256-GCM.
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
        throw std::runtime_error("EVP_EncryptInit_ex failed in secure_encrypt_string");

    // Initialize key and IV.
    if (EVP_EncryptInit_ex(ctx, NULL, NULL,
                           reinterpret_cast<const unsigned char*>(key.data()), iv) != 1)
        throw std::runtime_error("EVP_EncryptInit_ex (key/IV) failed in secure_encrypt_string");

    // Encrypt the plaintext.
    std::vector<unsigned char> ciphertext(plaintext.size() + AES_GCM_TAG_SIZE);
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptUpdate failed in secure_encrypt_string");
    }
    int ciphertext_len = len;

    // Finalize encryption.
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed in secure_encrypt_string");
    }
    ciphertext_len += len;

    // Retrieve the GCM authentication tag.
    unsigned char tag[AES_GCM_TAG_SIZE];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, tag) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CIPHER_CTX_ctrl (get tag) failed in secure_encrypt_string");
    }
    EVP_CIPHER_CTX_free(ctx);

    // Build output: IV || ciphertext || tag.
    std::string output(reinterpret_cast<char*>(iv), AES_GCM_IV_SIZE);
    output.append(reinterpret_cast<char*>(ciphertext.data()), ciphertext_len);
    output.append(reinterpret_cast<char*>(tag), AES_GCM_TAG_SIZE);

    return output;
}

// secure_decrypt_string:
// Decrypts the ciphertext using AES-256-GCM and verifies its authentication tag.
// Expects the input format: [IV (12 bytes)] || [ciphertext] || [tag (16 bytes)].
inline std::string secure_decrypt_string(const std::string& input) {
    const std::string& key = KeyManager::getKey();
    if (input.size() < AES_GCM_IV_SIZE + AES_GCM_TAG_SIZE)
        throw std::runtime_error("Input too short in secure_decrypt_string");

    size_t iv_offset = 0;
    size_t ciphertext_offset = AES_GCM_IV_SIZE;
    size_t tag_offset = input.size() - AES_GCM_TAG_SIZE;
    size_t ciphertext_len = tag_offset - AES_GCM_IV_SIZE;

    const unsigned char* iv = reinterpret_cast<const unsigned char*>(input.data());
    const unsigned char* ciphertext = reinterpret_cast<const unsigned char*>(input.data() + ciphertext_offset);
    const unsigned char* tag = reinterpret_cast<const unsigned char*>(input.data() + tag_offset);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("EVP_CIPHER_CTX_new failed in secure_decrypt_string");

    // Initialize decryption context with AES-256-GCM.
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed in secure_decrypt_string");
    }

    // Initialize key and IV.
    if (EVP_DecryptInit_ex(ctx, NULL, NULL,
                           reinterpret_cast<const unsigned char*>(key.data()), iv) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex (key/IV) failed in secure_decrypt_string");
    }

    // Decrypt the ciphertext.
    std::vector<unsigned char> plaintext(ciphertext_len + AES_GCM_TAG_SIZE);
    int len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext, ciphertext_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptUpdate failed in secure_decrypt_string");
    }
    int plaintext_len = len;

    // Set the expected GCM authentication tag.
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, const_cast<unsigned char*>(tag)) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CIPHER_CTX_ctrl (set tag) failed in secure_decrypt_string");
    }

    // Finalize decryption; if authentication fails, this will return <= 0.
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) <= 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption failed: Authentication tag verification failed in secure_decrypt_string");
    }
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);
}

#endif 
