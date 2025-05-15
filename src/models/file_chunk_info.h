#pragma once

#include <nlohmann/json.hpp>
#include <string>

struct FileChunkInfo {
    std::string file_id;        // 文件唯一标识符
    std::string file_name;      // 文件名
    size_t file_size;           // 文件总大小
    size_t chunk_size;          // 块大小
    size_t total_chunks;        // 总块数
    std::string file_checksum;  // 整个文件的校验和
    size_t current_chunk;       // 当前块编号
    std::string chunk_checksum; // 当前块的校验和
    std::string chunk_data;     // 块数据（Base64编码）

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileChunkInfo,
                                   file_id,
                                   file_name,
                                   file_size,
                                   chunk_size,
                                   total_chunks,
                                   file_checksum,
                                   current_chunk,
                                   chunk_checksum,
                                   chunk_data);
};