#pragma once

#include <models/binary_message_header.h>
#include <models/message_type.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace lansend {

class BinaryMessageHelper {
public:
    static std::vector<uint8_t> Create(MessageType type,
                                       const nlohmann::json& metadata,
                                       const std::vector<uint8_t>& data = {}) {
        std::string metadata_str = metadata.dump();

        size_t total_size = sizeof(BinaryMessageHeader) + metadata_str.size() + data.size();
        std::vector<uint8_t> result(total_size);

        BinaryMessageHeader header;
        header.message_type = static_cast<uint8_t>(type);
        header.metadata_size = htonl(
            static_cast<uint32_t>(metadata_str.size())); // Network byte order

        std::memcpy(result.data(), &header, sizeof(header));

        std::memcpy(result.data() + sizeof(header), metadata_str.data(), metadata_str.size());

        if (!data.empty()) {
            std::memcpy(result.data() + sizeof(header) + metadata_str.size(),
                        data.data(),
                        data.size());
        }

        return result;
    }

    static bool Parse(const std::vector<uint8_t>& message,
                      MessageType& type,
                      nlohmann::json& metadata,
                      std::vector<uint8_t>& data) {
        if (message.size() < sizeof(BinaryMessageHeader)) {
            return false;
        }

        BinaryMessageHeader header;
        std::memcpy(&header, message.data(), sizeof(header));

        type = static_cast<MessageType>(header.message_type);

        // Get metadata size from network byte order
        uint32_t metadata_size = ntohl(header.metadata_size);

        if (message.size() < sizeof(header) + metadata_size) {
            return false;
        }

        std::string metadata_str(reinterpret_cast<const char*>(message.data() + sizeof(header)),
                                 metadata_size);
        try {
            metadata = nlohmann::json::parse(metadata_str);
        } catch (const nlohmann::json::exception& e) {
            spdlog::error("Failed to parse metadata JSON: {}", e.what());
            return false;
        }

        size_t data_size = message.size() - sizeof(header) - metadata_size;
        if (data_size > 0) {
            data.resize(data_size);
            std::memcpy(data.data(), message.data() + sizeof(header) + metadata_size, data_size);
        } else {
            data.clear();
        }

        return true;
    }
};

} // namespace lansend
