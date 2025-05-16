#pragma once

#include "file_hasher.hpp"
#include <boost/asio.hpp>
#include <constants/transfer.h>
#include <models/file_chunk_info.h>
#include <network/http_client.h>
#include <security/certificate_manager.hpp>

namespace lansend {

using ProgressCallback = std::function<void(size_t bytesTransferred, size_t totalBytes)>;

class FileSender {
public:
    FileSender(boost::asio::io_context& ioc, CertificateManager& cert_manager);
    ~FileSender() = default;

    boost::asio::awaitable<bool> SendFile(const std::string& host,
                                          unsigned short port,
                                          const std::filesystem::path& file_path,
                                          size_t chunk_size = transfer::kDefaultChunkSize,
                                          ProgressCallback progress_callback = nullptr);

    boost::asio::awaitable<bool> CancelSend(const std::string& file_id);

private:
    boost::asio::awaitable<std::string> PrepareSend(const FileChunkInfo& file_info);

    boost::asio::awaitable<bool> SendChunk(FileChunkInfo& chunk_info, const std::string& chunk_data);

    boost::asio::awaitable<bool> VerifyAndComplete(const std::string& file_id);

    HttpsClient client_;

    CertificateManager& cert_manager_;

    FileHasher file_hasher_;

    boost::asio::io_context& ioc_;
};

} // namespace lansend