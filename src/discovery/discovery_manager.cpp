#include "../util/logger.hpp"
#include <boost/asio/use_awaitable.hpp>
#include "discovery_manager.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <iostream>
#include <mutex>
#include <boost/asio/ip/udp.hpp>
#include "../models/device_info.hpp"
#include "../models/transfer_progress.hpp"

using namespace boost::asio;

DiscoveryManager::DiscoveryManager(io_context& ioc)
    : io_context_(ioc),
      broadcast_socket_(ioc),
      listen_socket_(ioc),
      broadcast_timer_(ioc),
      device_found_callback_([](const lansend::models::DeviceInfo& device) {
          std::cout << "Device found: " << device.id << std::endl;
      }),
      device_lost_callback_([](const std::string& device_id) {
          std::cout << "Device lost: " << device_id << std::endl;
      });

DiscoveryManager::~DiscoveryManager() {
    stop();
}

void DiscoveryManager::start(uint16_t port) {
    try {
        // 初始化广播套接字
        broadcast_socket_.open(ip::udp::v4());
        broadcast_socket_.set_option(socket_base::broadcast(true));
        broadcast_socket_.set_option(socket_base::reuse_address(true));

        // 初始化监听套接字
        ip::udp::endpoint listen_endpoint(ip::udp::v4(), port);
        listen_socket_.open(listen_endpoint.protocol());
        listen_socket_.set_option(socket_base::reuse_address(true));
        listen_socket_.bind(listen_endpoint);

        // 启动广播和监听协程
        co_spawn(io_context_, broadcaster(), detached);
        co_spawn(io_context_, listener(), detached);
    } catch (const std::exception& e) {
        std::cerr << "Error starting DiscoveryManager: " << e.what() << std::endl;
    }
}

void DiscoveryManager::stop() {
    try {
        if (broadcast_socket_.is_open()) {
            broadcast_socket_.close();
        }
        if (listen_socket_.is_open()) {
            listen_socket_.close();
        }
        broadcast_timer_.cancel();
    } catch (const std::exception& e) {
        std::cerr << "Error stopping DiscoveryManager: " << e.what() << std::endl;
    }
}

void DiscoveryManager::add_device(const DeviceInfo& device) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    auto it = discovered_devices_.find(device.id);
    if (it == discovered_devices_.end()) {
        discovered_devices_[device.id] = device;
        if (device_found_callback_) {
            device_found_callback_(device);
        }
    } else {
        it->second.last_seen = std::chrono::system_clock::now();
    }
}

void DiscoveryManager::remove_device(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    auto it = discovered_devices_.find(device_id);
    if (it != discovered_devices_.end()) {
        discovered_devices_.erase(it);
        if (device_lost_callback_) {
            device_lost_callback_(device_id);
        }
    }
}

std::vector<DiscoveryManager::DeviceInfo> DiscoveryManager::get_devices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    std::vector<DeviceInfo> devices;
    for (const auto& pair : discovered_devices_) {
        devices.push_back(pair.second);
    }
    return devices;
}

void DiscoveryManager::set_device_found_callback(std::function<void(const DeviceInfo&)> callback) {
    device_found_callback_ = callback;
}

void DiscoveryManager::set_device_lost_callback(std::function<void(const std::string&)> callback) {
    device_lost_callback_ = callback;
}

awaitable<void> DiscoveryManager::broadcaster() {
    try {
        const std::string broadcast_address = "255.255.255.255";
        const uint16_t broadcast_port = 37020; // 示例端口
        ip::udp::endpoint broadcast_endpoint(ip::make_address(broadcast_address), broadcast_port);

        // 模拟设备信息
        DeviceInfo self_device;
        self_device.id = "self_device_id";
        self_device.name = "Self Device";
        self_device.ip = "127.0.0.1";
        self_device.port = 37020;
        self_device.fingerprint = "device_fingerprint";
        self_device.last_seen = std::chrono::system_clock::now();

        // 序列化设备信息
        std::string data = self_device.id + "|" + self_device.name + "|" + self_device.ip + "|" +
                           std::to_string(self_device.port) + "|" + self_device.fingerprint;

        while (broadcast_socket_.is_open()) {
            co_await async_write_to(broadcast_socket_, buffer(data), broadcast_endpoint, use_awaitable);
            co_await broadcast_timer_.async_wait(use_awaitable);
            broadcast_timer_.expires_after(std::chrono::seconds(5)); // 每5秒广播一次
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in broadcaster: " << e.what() << std::endl;
    }
}

awaitable<void> DiscoveryManager::listener() {
    try {
        constexpr size_t buffer_size = 1024;
        std::array<char, buffer_size> recv_buffer;
        ip::udp::endpoint sender_endpoint;

        while (listen_socket_.is_open()) {
            size_t bytes_received = co_await listen_socket_.async_receive_from(
                buffer(recv_buffer), sender_endpoint, use_awaitable);

            std::string data(recv_buffer.data(), bytes_received);

            // 反序列化设备信息
            size_t pos1 = data.find('|');
            size_t pos2 = data.find('|', pos1 + 1);
            size_t pos3 = data.find('|', pos2 + 1);
            size_t pos4 = data.find('|', pos3 + 1);

            if (pos1 != std::string::npos && pos2 != std::string::npos &&
                pos3 != std::string::npos && pos4 != std::string::npos) {
                DeviceInfo device;
                device.id = data.substr(0, pos1);
                device.name = data.substr(pos1 + 1, pos2 - pos1 - 1);
                device.ip = data.substr(pos2 + 1, pos3 - pos2 - 1);
                device.port = static_cast<uint16_t>(std::stoi(data.substr(pos3 + 1, pos4 - pos3 - 1)));
                device.fingerprint = data.substr(pos4 + 1);
                device.last_seen = std::chrono::system_clock::now();

                add_device(device);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in listener: " << e.what() << std::endl;
    }
}
