#pragma once

#include <boost/asio/ssl/context.hpp>
#include <filesystem>
#include <models/security_context.hpp>
#include <openssl/x509.h>
#include <string>
#include <unordered_set>

class CertificateManager {
public:
    CertificateManager(const std::filesystem::path& certDir);
    ~CertificateManager();

    bool InitSecurityContext();

    const SecurityContext& security_context() const;

    static std::string CalculateCertificateHash(const std::string& certificatePem);

    bool VerifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx);

    void TrustFingerprint(const std::string& fingerprint);

    bool IsFingerprintTrusted(const std::string& fingerprint);

    bool TrustCertificate(const std::string& certificatePem);

private:
    bool GenerateSelfSignedCertificate();

    bool SaveSecurityContext();

    bool LoadSecurityContext();

    void LoadTrustedFingerprints();

    void SaveTrustedFingerprints();

    SecurityContext security_context_;
    std::filesystem::path certificate_dir_;
    std::filesystem::path trusted_fingerprints_path_;

    std::unordered_set<std::string> trusted_fingerprints_;

    static constexpr int kCertValidityDays = 3650;
};