#pragma once

#include "models/device_info.h"
#include "models/dto/file_dto.h"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <vector>

namespace lansend {

struct RequestSendDto {
    models::DeviceInfo device_info; // 发送方的设备信息
    std::vector<FileDto> files;    // 文件信息列表

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RequestSendDto, device_info, files);
};

} // namespace lansend
