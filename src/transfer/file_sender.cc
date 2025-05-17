#include "file_sender.h"
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <constants/route.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <utils/binary_message.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lansend {

FileSender::FileSender(net::io_context& ioc, CertificateManager& cert_manager)
    : ioc_(ioc)
    , cert_manager_(cert_manager)
    , client_(ioc, cert_manager) {}

net::awaitable<bool> FileSender::SendFile(const std::string& host,
                                          unsigned short port,
                                          const fs::path& file_path,
                                          size_t chunk_size,
                                          ProgressCallback progress_callback) {
    spdlog::debug("FileSender::SendFile");
    try {
        if (!fs::exists(file_path)) {
            spdlog::error("File not found: {}", file_path.string());
            co_return false;
        }

        size_t file_size = fs::file_size(file_path);
        if (file_size == 0) {
            spdlog::error("Empty file: {}", file_path.string());
            co_return false;
        }

        spdlog::info("Calculating file checksum...");
        std::string file_checksum = file_hasher_.CalculateFileChecksum(file_path);
        spdlog::info("File checksum: {}", file_checksum);

        boost::uuids::random_generator uuid_gen;
        std::string file_id = boost::uuids::to_string(uuid_gen());

        size_t total_chunks = (file_size + chunk_size - 1) / chunk_size;

        FileChunkInfo file_info;
        file_info.file_id = file_id;
        file_info.file_name = file_path.filename().string();
        file_info.file_size = file_size;
        file_info.chunk_size = chunk_size;
        file_info.total_chunks = total_chunks;
        file_info.file_checksum = file_checksum;

        spdlog::info("Starting large file transfer: {} ({} bytes, {} chunks)",
                     file_path.filename().string(),
                     file_size,
                     total_chunks);

        cert_manager_.set_current_hostname(host);

        bool connected = co_await client_.Connect(host, port);
        if (!connected) {
            spdlog::error("Failed to connect to server");
            cert_manager_.set_current_hostname("");
            co_return false;
        }

        std::string response_file_id = co_await PrepareSend(file_info);
        if (response_file_id.empty()) {
            spdlog::error("Failed to initiate file transfer");
            co_await client_.Disconnect();
            cert_manager_.set_current_hostname("");
            co_return false;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            spdlog::error("Cannot open file for reading: {}", file_path.string());
            co_await client_.Disconnect();
            cert_manager_.set_current_hostname("");
            co_return false;
        }

        std::vector<char> buffer(chunk_size);
        size_t total_sent = 0;
        bool all_chunks_sent = true;

        for (size_t chunk_idx = 0; chunk_idx < total_chunks; ++chunk_idx) {
            file.read(buffer.data(), chunk_size);
            size_t bytes_read = file.gcount();

            if (bytes_read == 0)
                break;

            FileChunkInfo chunk_info = file_info;
            chunk_info.current_chunk = chunk_idx;

            std::string chunk_data(buffer.data(), bytes_read);
            chunk_info.chunk_checksum = file_hasher_.CalculateDataChecksum(chunk_data);

            bool chunk_sent = co_await SendChunk(chunk_info, chunk_data);
            if (!chunk_sent) {
                spdlog::error("Failed to send chunk {}/{}", chunk_idx + 1, total_chunks);
                all_chunks_sent = false;
                break;
            }

            total_sent += bytes_read;

            if (progress_callback) {
                progress_callback(total_sent, file_size);
            }

            if ((chunk_idx + 1) % 10 == 0 || chunk_idx + 1 == total_chunks) {
                spdlog::info("Sent chunk {}/{} ({:.1f}%)",
                             chunk_idx + 1,
                             total_chunks,
                             100.0 * total_sent / file_size);
            }
        }

        file.close();

        if (all_chunks_sent) {
            bool finalized = co_await VerifyAndComplete(file_id);
            if (!finalized) {
                spdlog::error("Failed to finalize file transfer");
                co_await client_.Disconnect();
                cert_manager_.set_current_hostname("");
                co_return false;
            }
        }

        co_await client_.Disconnect();

        cert_manager_.set_current_hostname("");

        if (all_chunks_sent) {
            spdlog::info("Large file transfer completed successfully: {}", file_path.string());
            co_return true;
        } else {
            spdlog::error("Large file transfer failed: {}", file_path.string());
            co_return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error sending large file: {}", e.what());
    }
    // Ensure to close the file and disconnect
    try {
        if (client_.IsConnected()) {
            co_await client_.Disconnect();
        }
    } catch (...) {
        // Neglect any errors during disconnection
    }

    cert_manager_.set_current_hostname("");
    co_return false;
}

boost::asio::awaitable<bool> FileSender::CancelSend(const std::string& file_id) {
    spdlog::debug("FileSender::CancelSend");
    try {
        if (!client_.IsConnected()) {
            spdlog::error("Cannot cancel transfer: no active connection");
            co_return false;
        }

        json metadata;
        metadata["file_id"] = file_id;

        // std::vector<uint8_t> binary_message = BinaryMessageHelper::Create(MessageType::kFileEnd,
        //                                                                   metadata);

        auto req = client_.CreateRequest<http::string_body>(http::verb::post,
                                                            ApiRoute::kCancelSend.data(),
                                                            true);
        req.body() = metadata.dump();
        req.prepare_payload();

        spdlog::info("Sending cancel request for file ID: {}", file_id);
        auto res = co_await client_.SendRequest(req);

        if (res.result() != http::status::ok) {
            spdlog::error("Failed to cancel file transfer: {} {}",
                          static_cast<unsigned>(res.result()),
                          res.reason());
            co_return false;
        }

        json response_data = json::parse(res.body());
        bool success = response_data.value("success", false);
        if (success) {
            std::string message = response_data.value("message", "");
            spdlog::info("File transfer cancelled: {}", message);
            co_return true;
        } else {
            std::string error = response_data.value("error", "Unknown error");
            spdlog::error("Failed to cancel transfer: {}", error);
        }

        co_return false;
    } catch (const std::exception& e) {
        spdlog::error("Error while cancelling file transfer: {}", e.what());
        co_return false;
    }
}

net::awaitable<std::string> FileSender::PrepareSend(const FileChunkInfo& file_info) {
    spdlog::debug("FileSender::PrepareSend");
    try {
        if (!client_.IsConnected()) {
            throw std::runtime_error("No active connection for file transfer initialization");
        }

        json metadata = file_info;

        auto req = client_.CreateRequest<http::string_body>(http::verb::post,
                                                            ApiRoute::kPrepareSend.data(),
                                                            true);
        req.body() = metadata.dump();
        req.prepare_payload();

        spdlog::info("Sending prepare request for file transfer");
        auto res = co_await client_.SendRequest(req);

        if (res.result() != http::status::ok) {
            spdlog::error("Failed to initiate file transfer: {} {}",
                          static_cast<unsigned>(res.result()),
                          res.reason());
            co_return "";
        }
        spdlog::info("File transfer preparation result is ok");

        json response_data = json::parse(res.body());
        if (response_data.contains("file_id")) {
            std::string file_id = response_data["file_id"].get<std::string>();
            spdlog::info("File transfer initiated, file_id: {}", file_id);
            co_return file_id;
        }

        spdlog::error("Invalid response format");
        co_return "";
    } catch (const std::exception& e) {
        spdlog::error("Error initiating file transfer: {}", e.what());
        co_return "";
    }
}

net::awaitable<bool> FileSender::SendChunk(FileChunkInfo& chunk_info,
                                           const std::string& chunk_data) {
    spdlog::debug("FileSender::SendChunk");
    try {
        if (!client_.IsConnected()) {
            throw std::runtime_error("No active connection for sending chunk");
        }

        json metadata = chunk_info;

        BinaryMessage binary_message = CreateBinaryMessage(metadata,
                                                           BinaryData(chunk_data.begin(),
                                                                      chunk_data.end()));

        auto req = client_.CreateRequest<http::vector_body<uint8_t>>(http::verb::post,
                                                                     ApiRoute::kSendChunk.data(),
                                                                     true);
        req.body() = std::move(binary_message);
        req.prepare_payload();

        auto res = co_await client_.SendRequest(req);

        if (res.result() != http::status::ok) {
            spdlog::error("Failed to send chunk {}: {} {}",
                          chunk_info.current_chunk,
                          static_cast<unsigned>(res.result()),
                          res.reason());
            co_return false;
        }

        json response_data = json::parse(res.body());
        bool success = response_data.value("success", false);
        if (success) {
            spdlog::debug("Chunk {} sent successfully", chunk_info.current_chunk);
            co_return true;
        }

        spdlog::error("Invalid response format");
        co_return false;
    } catch (const std::exception& e) {
        spdlog::error("Error sending file chunk: {}", e.what());
        co_return false;
    }
}

net::awaitable<bool> FileSender::VerifyAndComplete(const std::string& file_id) {
    spdlog::debug("FileSender::VerifyAndComplete");
    try {
        if (!client_.IsConnected()) {
            throw std::runtime_error("No active connection for finalizing transfer");
        }

        json metadata;
        metadata["file_id"] = file_id;

        auto req = client_.CreateRequest<http::string_body>(http::verb::post,
                                                            ApiRoute::kVerifyAndComplete.data(),
                                                            false);
        req.body() = metadata.dump();
        req.prepare_payload();

        auto res = co_await client_.SendRequest(req);

        if (res.result() != http::status::ok) {
            spdlog::error("Failed to finalize file transfer: {} {}",
                          static_cast<unsigned>(res.result()),
                          res.reason());
            co_return false;
        }

        json response_data = json::parse(res.body());
        bool success = response_data.value("success", false);
        if (success) {
            std::string checksum = response_data.value("checksum", "");
            std::string file_path = response_data.value("path", "");
            spdlog::info("File transfer finalized, server checksum: {}, file path: {}",
                         checksum,
                         file_path);
        }
        co_return success;
    } catch (const std::exception& e) {
        spdlog::error("Error finalizing file transfer: {}", e.what());
        co_return false;
    }
}

} // namespace lansend