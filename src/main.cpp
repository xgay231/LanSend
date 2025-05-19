#include "constants/path.hpp"
#include "network/http_server.hpp"
#include "security/certificate_manager.h"
#include "transfer/file_sender.h"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace po = boost::program_options;

void print_usage() {
    std::cout << "LanSend - 局域网文件传输工具\n\n"
              << "用法:\n"
              << "  LanSend server [选项]                  - 启动文件接收服务器\n"
              << "  LanSend send <IP地址> <文件路径...>     - 发送文件到指定IP地址\n"
              << "  LanSend devices                        - 列出局域网上的设备\n"
              << "  LanSend config                         - 查看和修改配置\n"
              << "  LanSend help                           - 显示此帮助信息\n\n"
              << "选项:\n"
              << "  --port,-p N        指定端口 (默认: 8080)\n"
              << "  --save-dir,-s DIR  指定保存文件的目录\n";
}

int run_server(uint16_t port, const fs::path& save_dir) {
    try {
        // 创建并配置IO上下文
        boost::asio::io_context io_context;

        // 创建证书管理器
        CertificateManager cert_manager(lansend::path::kCertificateDir);
        if (!cert_manager.InitSecurityContext()) {
            spdlog::error("无法初始化安全上下文");
            return 1;
        }

        // 创建SSL上下文
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_server);

        // 配置SSL上下文
        ssl_ctx.use_certificate_chain(
            boost::asio::buffer(cert_manager.security_context().certificate_pem));
        ssl_ctx.use_private_key(boost::asio::buffer(cert_manager.security_context().private_key_pem),
                                boost::asio::ssl::context::pem);
        // 创建HTTP服务器
        lansend::HttpServer server(io_context, ssl_ctx);

        // 启动服务器
        server.start(port);

        std::cout << "服务器已启动，正在监听端口 " << port << "\n";
        std::cout << "文件将保存到: " << save_dir << "\n";
        std::cout << "按 Ctrl+C 停止服务器...\n";

        // 创建工作线程池
        const int num_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        // 保留一个线程用于主IO循环
        for (int i = 1; i < num_threads; ++i) {
            threads.emplace_back([&io_context]() {
                try {
                    io_context.run();
                } catch (const std::exception& e) {
                    spdlog::error("IO线程异常: {}", e.what());
                }
            });
        }

        // 在主线程中运行IO循环
        io_context.run();

        // 等待工作线程完成
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        return 0;
    } catch (const std::exception& e) {
        spdlog::error("服务器错误: {}", e.what());
        return 1;
    }
}

int send_files(const std::string& host, uint16_t port, const std::vector<fs::path>& file_paths) {
    try {
        // 验证文件路径
        for (const auto& path : file_paths) {
            if (!fs::exists(path)) {
                std::cerr << "错误: 文件不存在: " << path << "\n";
                return 1;
            }
        }

        // 创建IO上下文
        boost::asio::io_context io_context;

        // 初始化证书管理器
        CertificateManager cert_manager(lansend::path::kCertificateDir);
        if (!cert_manager.InitSecurityContext()) {
            spdlog::error("无法初始化安全上下文");
            return 1;
        }

        // 创建传输管理器
        lansend::FileSender file_sender(io_context, cert_manager);

        // 创建进度回调
        auto progress_callback = [](const nlohmann::json& message) {
            if (message.contains("type") && message["type"] == "progress") {
                double percentage = message.value("percentage", 0.0);
                std::string file_name = message.value("file_name", "");
                size_t bytes_sent = message.value("bytes_sent", 0);
                size_t total_bytes = message.value("total_bytes", 1);

                std::cout << "\r发送进度 [" << file_name << "]: " << std::fixed
                          << std::setprecision(1) << percentage << "% (" << bytes_sent / 1024
                          << "KB / " << total_bytes / 1024 << "KB)" << std::flush;
            } else if (message.contains("type") && message["type"] == "completed") {
                std::cout << "\n文件发送完成！\n";
            } else if (message.contains("type") && message["type"] == "error") {
                std::cout << "\n错误: " << message.value("message", "未知错误") << "\n";
            }
        };

        std::cout << "正在发送 " << file_paths.size() << " 个文件到 " << host << ":" << port
                  << "...\n";

        // 创建完成标志和互斥锁
        std::mutex mutex;
        std::condition_variable cv;
        bool completed = false;

        // 启动发送
        file_sender.SendFiles(host, port, file_paths, [&](const nlohmann::json& message) {
            progress_callback(message);

            // 如果发送完成或失败，通知主线程
            if (message.contains("type")
                && (message["type"] == "completed" || message["type"] == "error")) {
                std::unique_lock<std::mutex> lock(mutex);
                completed = true;
                cv.notify_one();
            }
        });

        // 运行IO上下文直到没有更多工作
        io_context.run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n发送错误: " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    lansend::Logger logger(
#ifdef LANSEND_DEBUG
        lansend::Logger::Level::debug,
#else
        lansend::Logger::Level::info,
#endif
        (lansend::path::kLogDir / "lansend.log").string());

    // 初始化配置
    lansend::init_config();
    spdlog::info("欢迎使用 LanSend!");

    try {
        // 检查命令行参数
        if (argc < 2) {
            std::cerr << "错误: 缺少参数\n";
            print_usage();
            return 1;
        }

        std::string command = argv[1];

        if (command == "server") {
            // 解析服务器模式参数
            po::options_description desc("服务器选项");
            desc.add_options()("port,p",
                               po::value<uint16_t>()->default_value(8080),
                               "监听端口")("save-dir,s",
                                           po::value<std::string>()->default_value(
                                               lansend::path::kSystemDownloadDir.string()),
                                           "保存文件目录");

            po::variables_map vm;
            po::store(po::command_line_parser(argc - 1, argv + 1).options(desc).run(), vm);
            po::notify(vm);

            uint16_t port = vm["port"].as<uint16_t>();
            fs::path save_dir = vm["save-dir"].as<std::string>();

            return run_server(port, save_dir);
        } else if (command == "send") {
            // 发送模式必须至少有目标主机和一个文件参数
            if (argc < 4) {
                std::cerr << "错误: 发送命令需要指定IP地址和至少一个文件路径\n";
                print_usage();
                return 1;
            }

            std::string host = argv[2];
            uint16_t port = 8080; // 默认端口

            // 检查主机名是否包含端口
            auto pos = host.find(':');
            if (pos != std::string::npos) {
                try {
                    port = static_cast<uint16_t>(std::stoi(host.substr(pos + 1)));
                    host = host.substr(0, pos);
                } catch (...) {
                    std::cerr << "错误: 端口格式不正确\n";
                    return 1;
                }
            }

            // 收集所有文件路径
            std::vector<fs::path> file_paths;
            for (int i = 3; i < argc; ++i) {
                file_paths.emplace_back(argv[i]);
            }

            return send_files(host, port, file_paths);
        } else if (command == "devices") {
            std::cout << "设备发现功能尚未实现\n";
            return 0;
        } else if (command == "config") {
            std::cout << "配置信息:\n";
            std::cout << " - 设备ID: " << lansend::settings.device_id << "\n";
            std::cout << " - 设备别名: " << lansend::settings.alias << "\n";
            std::cout << " - 默认端口: " << lansend::settings.port << "\n";
            std::cout << " - 下载目录: " << lansend::settings.saveDir << "\n";
            return 0;
        } else if (command == "help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "错误: 未知命令 '" << command << "'\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << "\n";
        return 1;
    }

    lansend::save_config();
    return 0;
}