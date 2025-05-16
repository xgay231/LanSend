#include "receive_controller.h"
#include "api/http_server.hpp"
#include "constants/route.hpp"
#include <boost/beast/http/string_body_fwd.hpp>
#include <fstream>
#include <models/binary_message_header.h>
#include <models/message_type.h>
#include <nlohmann/json.hpp>
#include <utils/binary_message_helper.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lansend {

ReceiveController::ReceiveController(api::HttpServer& server, const std::filesystem::path& save_dir)
    : server_(server)
    , save_dir_(save_dir) {
    if (!std::filesystem::exists(save_dir_)) {
        std::filesystem::create_directories(save_dir_);
    }
    InstallRoutes();
}

net::awaitable<http::response<http::string_body>> ReceiveController::OnPrepareSend(
    const http::request<http::vector_body<std::uint8_t>>& req) {
    http::response<http::vector_body<std::uint8_t>> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::connection, "keep-alive");

    try {
        const auto& binary_data = req.body();

        MessageType msg_type;
        json metadata;
        std::vector<std::uint8_t> file_data;

        if (!BinaryMessageHelper::Parse(binary_data, msg_type, metadata, file_data)) {
            throw std::runtime_error("Failed to parse binary message");
        }

        if (msg_type != MessageType::kFileStart) {
            throw std::runtime_error("Unexpected message type");
        }

        FileChunkInfo file_info;
        nlohmann::from_json(metadata, file_info);

        fs::path temp_file = save_dir_ / (file_info.file_id + ".part");

        {
            std::lock_guard<std::mutex> lock(mutex_);

            active_transfers_[file_info.file_id] = temp_file;
            file_metadata_[file_info.file_id] = file_info;
            received_chunks_[file_info.file_id] = {};
        }

        json response_metadata;
        response_metadata["file_id"] = file_info.file_id;
        response_metadata["success"] = true;

        std::vector<std::uint8_t> response_binary
            = BinaryMessageHelper::Create(MessageType::kFileStart, response_metadata);

        res.body() = std::move(response_binary);

        spdlog::info("Started receiving file {} ({}), {} chunks expected",
                     file_info.file_name,
                     file_info.file_size,
                     file_info.total_chunks);
    } catch (const std::exception& e) {
        spdlog::error("Error processing file start request: {}", e.what());

        json error_metadata;
        error_metadata["success"] = false;
        error_metadata["error"] = e.what();

        std::vector<std::uint8_t> error_binary = BinaryMessageHelper::Create(MessageType::kError,
                                                                             error_metadata);

        res.result(http::status::bad_request);
        res.body() = std::move(error_binary);
    }

    res.prepare_payload();
    co_return res;
}

net::awaitable<http::response<http::string_body>> ReceiveController::OnSendChunk(
    const http::request<http::vector_body<std::uint8_t>>& req) {
    http::response<http::vector_body<std::uint8_t>> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::connection, "keep-alive");

    try {
        const auto& binary_data = req.body();

        MessageType msg_type;
        json metadata;
        std::vector<std::uint8_t> chunk_data;

        if (!BinaryMessageHelper::Parse(binary_data, msg_type, metadata, chunk_data)) {
            throw std::runtime_error("Failed to parse binary message");
        }

        if (msg_type != MessageType::kFileChunk) {
            throw std::runtime_error("Unexpected message type");
        }

        FileChunkInfo chunk_info;
        nlohmann::from_json(metadata, chunk_info);

        std::string actual_checksum = file_hasher_.CalculateDataChecksum(
            std::string(reinterpret_cast<const char*>(chunk_data.data()), chunk_data.size()));

        if (actual_checksum != chunk_info.chunk_checksum) {
            throw std::runtime_error("Chunk checksum mismatch");
        }

        fs::path temp_file;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (active_transfers_.find(chunk_info.file_id) == active_transfers_.end()) {
                throw std::runtime_error("Invalid file_id: " + chunk_info.file_id);
            }

            temp_file = active_transfers_[chunk_info.file_id];

            // Check if the chunk has already been received
            auto& received_set = received_chunks_[chunk_info.file_id];
            if (received_set.find(chunk_info.current_chunk) != received_set.end()) {
                spdlog::warn("Duplicate chunk received: {}", chunk_info.current_chunk);

                json response_metadata;
                response_metadata["success"] = true;
                response_metadata["chunk"] = chunk_info.current_chunk;

                std::vector<std::uint8_t> response_binary
                    = BinaryMessageHelper::Create(MessageType::kFileChunkAck, response_metadata);

                res.body() = std::move(response_binary);
                res.prepare_payload();
                co_return res;
            }
        }

        std::fstream file(temp_file, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) {
            file.open(temp_file, std::ios::binary | std::ios::out);
            if (!file) {
                throw std::runtime_error("Failed to create temporary file: " + temp_file.string());
            }
        }

        size_t offset = chunk_info.current_chunk * chunk_info.chunk_size;
        file.seekp(offset, std::ios::beg);
        file.write(reinterpret_cast<const char*>(chunk_data.data()), chunk_data.size());

        if (!file) {
            throw std::runtime_error("Failed to write chunk data to file");
        }

        file.close();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Mark the chunk as received
            received_chunks_[chunk_info.file_id].insert(chunk_info.current_chunk);

            const FileChunkInfo& file_info = file_metadata_[chunk_info.file_id];

            size_t received_count = received_chunks_[chunk_info.file_id].size();
            if (received_count % 10 == 0 || received_count == file_info.total_chunks) {
                spdlog::info("Received chunk {}/{} for file {} ({:.1f}%)",
                             received_count,
                             file_info.total_chunks,
                             file_info.file_name,
                             100.0 * received_count / file_info.total_chunks);
            }
        }

        json response_metadata;
        response_metadata["success"] = true;
        response_metadata["chunk"] = chunk_info.current_chunk;

        std::vector<std::uint8_t> response_binary
            = BinaryMessageHelper::Create(MessageType::kFileChunkAck, response_metadata);

        res.body() = std::move(response_binary);
    } catch (const std::exception& e) {
        spdlog::error("Error processing file chunk: {}", e.what());

        json error_metadata;
        error_metadata["success"] = false;
        error_metadata["error"] = e.what();

        std::vector<std::uint8_t> error_binary = BinaryMessageHelper::Create(MessageType::kError,
                                                                             error_metadata);

        res.result(http::status::bad_request);
        res.body() = std::move(error_binary);
    }

    res.prepare_payload();
    co_return res;
}

net::awaitable<http::response<http::string_body>> ReceiveController::OnVerifyAndComplete(
    const http::request<http::vector_body<std::uint8_t>>& req) {
    http::response<http::vector_body<std::uint8_t>> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::connection, "keep-alive");

    try {
        const auto& binary_data = req.body();

        MessageType msg_type;
        json metadata;
        std::vector<std::uint8_t> unused_data;

        if (!BinaryMessageHelper::Parse(binary_data, msg_type, metadata, unused_data)) {
            throw std::runtime_error("Failed to parse binary message");
        }

        if (msg_type != MessageType::kFileEnd) {
            throw std::runtime_error("Unexpected message type");
        }

        std::string file_id = metadata["file_id"].get<std::string>();

        FileChunkInfo file_info;
        fs::path temp_file;
        bool all_chunks_received = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (active_transfers_.find(file_id) == active_transfers_.end()) {
                throw std::runtime_error("Invalid file_id: " + file_id);
            }

            temp_file = active_transfers_[file_id];
            file_info = file_metadata_[file_id];

            // Check if all chunks have been received
            size_t received_count = received_chunks_[file_id].size();
            all_chunks_received = (received_count == file_info.total_chunks);

            if (!all_chunks_received) {
                spdlog::error("Not all chunks received: {}/{}",
                              received_count,
                              file_info.total_chunks);
                throw std::runtime_error("Not all chunks received");
            }
        }

        // Verify file checksum
        std::string actual_checksum = file_hasher_.CalculateFileChecksum(temp_file);
        if (actual_checksum != file_info.file_checksum) {
            spdlog::error("File checksum mismatch: {} != {}",
                          actual_checksum,
                          file_info.file_checksum);
            throw std::runtime_error("File checksum mismatch");
        }

        fs::path final_path = save_dir_ / file_info.file_name;

        // Add suffix if file already exists
        if (fs::exists(final_path)) {
            std::string stem = final_path.stem().string();
            std::string ext = final_path.extension().string();
            final_path = save_dir_ / (stem + "_" + file_id.substr(0, 8) + ext);
        }

        fs::rename(temp_file, final_path);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_transfers_.erase(file_id);
            file_metadata_.erase(file_id);
            received_chunks_.erase(file_id);
        }

        spdlog::info("File transfer completed: {}, saved to {}",
                     file_info.file_name,
                     final_path.string());

        json response_metadata;
        response_metadata["success"] = true;
        response_metadata["file_id"] = file_id;
        response_metadata["checksum"] = actual_checksum;
        response_metadata["path"] = final_path.string();

        std::vector<std::uint8_t> response_binary
            = BinaryMessageHelper::Create(MessageType::kComplete, response_metadata);

        res.body() = std::move(response_binary);
    } catch (const std::exception& e) {
        spdlog::error("Error finalizing file transfer: {}", e.what());

        json error_metadata;
        error_metadata["success"] = false;
        error_metadata["error"] = e.what();

        std::vector<std::uint8_t> error_binary = BinaryMessageHelper::Create(MessageType::kError,
                                                                             error_metadata);

        res.result(http::status::bad_request);
        res.body() = std::move(error_binary);
    }

    res.prepare_payload();
    co_return res;
}

net::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
ReceiveController::OnCancelSend(
    const http::request<boost::beast::http::vector_body<std::uint8_t>>& req) {
    http::response<http::vector_body<std::uint8_t>> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::connection, "keep-alive");

    try {
        const auto& binary_data = req.body();
        MessageType msg_type;
        json metadata;
        std::vector<std::uint8_t> unused_data;

        if (!BinaryMessageHelper::Parse(binary_data, msg_type, metadata, unused_data)) {
            throw std::runtime_error("Failed to parse binary message");
        }

        std::string file_id = metadata["file_id"].get<std::string>();

        std::filesystem::path temp_file;
        bool found = false;
        std::string file_name;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = active_transfers_.find(file_id);
            if (it != active_transfers_.end()) {
                temp_file = it->second;
                found = true;

                if (file_metadata_.find(file_id) != file_metadata_.end()) {
                    file_name = file_metadata_[file_id].file_name;
                    spdlog::info("Cancelling transfer for file: {}", file_name);
                } else {
                    spdlog::info("Cancelling transfer for file ID: {}", file_id);
                }

                active_transfers_.erase(it);
                file_metadata_.erase(file_id);
                received_chunks_.erase(file_id);
            }
        }

        if (found && std::filesystem::exists(temp_file)) {
            std::error_code ec;
            std::filesystem::remove(temp_file, ec);
            if (ec) {
                spdlog::warn("Failed to delete temporary file: {}, error: {}",
                             temp_file.string(),
                             ec.message());
            } else {
                spdlog::info("Temporary file deleted: {}", temp_file.string());
            }
        }

        json response_metadata;
        response_metadata["success"] = true;
        response_metadata["file_id"] = file_id;
        if (!file_name.empty()) {
            response_metadata["file_name"] = file_name;
        }
        response_metadata["message"] = found ? "Transfer cancelled" : "Transfer not found";

        std::vector<std::uint8_t> response_binary
            = BinaryMessageHelper::Create(MessageType::kComplete, response_metadata);

        res.body() = std::move(response_binary);
    } catch (const std::exception& e) {
        spdlog::error("Error cancelling file transfer: {}", e.what());

        json error_metadata;
        error_metadata["success"] = false;
        error_metadata["error"] = e.what();

        std::vector<std::uint8_t> error_binary = BinaryMessageHelper::Create(MessageType::kError,
                                                                             error_metadata);

        res.result(http::status::bad_request);
        res.body() = std::move(error_binary);
    }

    res.prepare_payload();
    co_return res;
}

void ReceiveController::SetSaveDirectory(const std::filesystem::path& save_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    save_dir_ = save_dir;
    if (!std::filesystem::exists(save_dir_)) {
        std::filesystem::create_directories(save_dir_);
    }
}

void ReceiveController::InstallRoutes() {
    server_.add_route(ApiRoute::kPrepareSend.data(),
                      http::verb::post,
                      [this](http::request<http::vector_body<std::uint8_t>>&& req)
                          -> net::awaitable<api::AnyResponse> {
                          co_return this->OnPrepareSend(std::move(req));
                      });
    server_.add_route(ApiRoute::kSendChunk.data(),
                      http::verb::post,
                      [this](http::request<http::vector_body<std::uint8_t>>&& req)
                          -> net::awaitable<api::AnyResponse> {
                          co_return this->OnSendChunk(std::move(req));
                      });
    server_.add_route(ApiRoute::kVerifyAndComplete.data(),
                      http::verb::post,
                      [this](http::request<http::vector_body<std::uint8_t>>&& req)
                          -> net::awaitable<api::AnyResponse> {
                          co_return this->OnVerifyAndComplete(std::move(req));
                      });
    server_.add_route(ApiRoute::kCancelSend.data(),
                      http::verb::post,
                      [this](http::request<http::vector_body<std::uint8_t>>&& req)
                          -> net::awaitable<api::AnyResponse> {
                          co_return this->OnCancelSend(std::move(req));
                      });
}

} // namespace lansend