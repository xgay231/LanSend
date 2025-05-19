#pragma once

#include "utils/binary_message.h"
#include <filesystem>
#include <memory>
#include <openssl/evp.h>
#include <spdlog/spdlog.h>
#include <string>

namespace lansend {

class FileHasher {
public:
    FileHasher();
    ~FileHasher() = default;

    std::string CalculateFileChecksum(const std::filesystem::path& file_path);
    std::string CalculateDataChecksum(const lansend::BinaryData& data);

private:
    struct SHA256HashContext {
        EVP_MD_CTX* md_ctx_ = nullptr;
        const EVP_MD* md_type_ = nullptr;

        SHA256HashContext() {
            md_type_ = EVP_sha256();
            md_ctx_ = EVP_MD_CTX_new();
            if (!md_ctx_) {
                spdlog::error("FileHasher: Failed to create EVP_MD_CTX.");
            }
            if (!EVP_DigestInit_ex(md_ctx_, md_type_, nullptr)) {
                spdlog::error("FileHasher: Failed to initialize EVP_MD_CTX.");
                EVP_MD_CTX_free(md_ctx_);
                md_ctx_ = nullptr;
            }
        }

        ~SHA256HashContext() {
            if (md_ctx_) {
                EVP_MD_CTX_free(md_ctx_);
                md_ctx_ = nullptr;
            }
        }

        SHA256HashContext(const SHA256HashContext&) = delete;
        SHA256HashContext& operator=(const SHA256HashContext&) = delete;
    };
    std::unique_ptr<SHA256HashContext> sha256_hash_context_;
};

} // namespace lansend