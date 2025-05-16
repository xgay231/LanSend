// src/transfer/file_hasher.cpp
#include "file_hasher.hpp"
#include <fstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sstream>

FileHasher::FileHasher()
    : sha256_hash_context_(std::make_unique<SHA256HashContext>()) {
    if (!sha256_hash_context_ || !sha256_hash_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSLContext unique_ptr is valid, but internal md_ctx_ is "
                      "null. Construction failed.");
        sha256_hash_context_.reset();
    }
}

std::string FileHasher::CalculateFileChecksum(const std::filesystem::path& file_path) {
    if (!sha256_hash_context_ || !sha256_hash_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSL context not available for sync calculation (nullptr or "
                      "md_ctx_ is null).");
        return "";
    }
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file for checksum calculation");
    }

    constexpr size_t buffer_size = 8192;
    std::vector<char> buffer(buffer_size);

    EVP_DigestInit_ex(sha256_hash_context_->md_ctx_, sha256_hash_context_->md_type_, nullptr);

    while (file) {
        file.read(buffer.data(), buffer_size);
        size_t bytes_read = file.gcount();
        if (bytes_read > 0) {
            EVP_DigestUpdate(sha256_hash_context_->md_ctx_, buffer.data(), bytes_read);
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestFinal_ex(sha256_hash_context_->md_ctx_, hash, &hash_len);

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

std::string FileHasher::CalculateDataChecksum(const std::string& data) {
    if (!sha256_hash_context_ || !sha256_hash_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSL context not available for sync calculation (nullptr or "
                      "md_ctx_ is null).");
        return "";
    }

    EVP_DigestInit_ex(sha256_hash_context_->md_ctx_, sha256_hash_context_->md_type_, nullptr);

    EVP_DigestUpdate(sha256_hash_context_->md_ctx_, data.data(), data.size());

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestFinal_ex(sha256_hash_context_->md_ctx_, hash, &hash_len);

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}