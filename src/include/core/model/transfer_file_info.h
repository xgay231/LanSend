#pragma once

#include <filesystem>
#include <string>

namespace lansend {

struct TransferFileInfo {
    std::filesystem::path file_path;
    size_t file_size;
    size_t total_chunks;
    std::string file_token;
};

} // namespace lansend