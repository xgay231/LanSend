#pragma once

#include "send_session_manager.h"
#include <boost/asio/io_context.hpp>
#include <core/security/certificate_manager.h>

namespace lansend {

class HttpClientService {
public:
    HttpClientService(boost::asio::io_context& ioc, CertificateManager& cert_manager);
    ~HttpClientService() = default;

    void Ping(std::string_view host, unsigned short port);

    void ConnectDevice(const std::string& auth_code, const std::string& device_info);

    void SendFiles(std::string_view,
                   unsigned short port,
                   const std::vector<std::filesystem::path>& file_paths);

    void CancelSend(const std::string& session_id);

private:
    boost::asio::io_context& ioc_;
    CertificateManager& cert_manager_;
    SendSessionManager send_session_manager_;
};

} // namespace lansend