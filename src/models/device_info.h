#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utils/config.hpp>
#include <utils/system.h>

namespace lansend {
namespace models {

struct DeviceInfo {
    std::string device_id;        // 唯一ID
    std::string alias;            // 设备昵称
    std::string hostname;         // 主机名
    std::string operating_system; // 操作系统
    std::string ip_address;
    uint16_t port;

    static auto& LocalDeviceInfo() {
        static DeviceInfo local_device_info = []() {
            DeviceInfo info;
            info.device_id = settings.device_id;
            info.alias = settings.alias;
            info.hostname = system::Hostname();
            info.operating_system = system::OperatingSystem();
            info.ip_address = "127.0.0.1";
            info.port = settings.port;
            return info;
        }();
        if (local_device_info.alias != settings.alias) {
            local_device_info.alias = settings.alias;
        }
        auto addr = system::PublicIpv4Address();
        if (local_device_info.ip_address != addr) {
            local_device_info.ip_address = addr;
        }
        if (local_device_info.port != settings.port) {
            local_device_info.port = settings.port;
        }
        return local_device_info;
    }

    // 序列化/反序列化函数
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        DeviceInfo, device_id, alias, hostname, operating_system, ip_address, port)
};

} // namespace models
} // namespace lansend