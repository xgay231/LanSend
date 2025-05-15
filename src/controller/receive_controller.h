#pragma once

#include "api/http_server.hpp"
#include "transfer/file_hasher.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <filesystem>
#include <models/file_chunk_info.h>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace lansend {

class ReceiveController {
public:
    using FileId = std::string;

    ReceiveController(api::HttpServer& server, const std::filesystem::path& save_dir);
    ~ReceiveController() = default;

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
    OnPrepareSend(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> OnSendChunk(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
    OnVerifyAndComplete(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> OnCancelSend(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

    void SetSaveDirectory(const std::filesystem::path& save_dir);

private:
    void InstallRoutes();

    api::HttpServer& server_;
    std::filesystem::path save_dir_;
    std::unordered_map<FileId, std::filesystem::path> active_transfers_;
    std::unordered_map<FileId, FileChunkInfo> file_metadata_;
    std::unordered_map<FileId, std::unordered_set<size_t>> received_chunks_;
    FileHasher file_hasher_;
    std::mutex mutex_;
};

} // namespace lansend