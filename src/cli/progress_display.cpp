#include "progress_display.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

// 构造函数
ProgressDisplay::ProgressDisplay() {
    // 暂时为空
}

// 析构函数
ProgressDisplay::~ProgressDisplay() {
    // 暂时为空
}

// 私有方法，用于打印进度信息
void ProgressDisplay::print_progress(const lansend::models::TransferProgress& progress) {
    // 计算进度百分比
    double percentage = static_cast<double>(progress.bytes_transferred) / progress.total_bytes * 100.0;
    // 转换速度为合适的单位
    std::stringstream speed_ss;
    if (progress.speed < 1024) {
        speed_ss << std::fixed << std::setprecision(2) << progress.speed << " B/s";
    } else if (progress.speed < 1024 * 1024) {
        speed_ss << std::fixed << std::setprecision(2) << progress.speed / 1024.0 << " KB/s";
    } else if (progress.speed < 1024 * 1024 * 1024) {
        speed_ss << std::fixed << std::setprecision(2) << progress.speed / (1024.0 * 1024.0) << " MB/s";
    } else {
        speed_ss << std::fixed << std::setprecision(2) << progress.speed / (1024.0 * 1024.0 * 1024.0) << " GB/s";
    }
    std::string speed_str = speed_ss.str();

    // 计算剩余时间（秒）
    uint64_t remaining_bytes = progress.total_bytes - progress.bytes_transferred;
    double remaining_seconds = (progress.speed > 0) ? static_cast<double>(remaining_bytes) / progress.speed : 0;

    // 格式化剩余时间
    int hours = static_cast<int>(remaining_seconds / 3600);
    int minutes = static_cast<int>((remaining_seconds - hours * 3600) / 60);
    int seconds = static_cast<int>(remaining_seconds - hours * 3600 - minutes * 60);
    std::stringstream remaining_time_ss;
    if (hours > 0) {
        remaining_time_ss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        remaining_time_ss << minutes << "m ";
    }
    remaining_time_ss << seconds << "s";
    std::string remaining_time_str = remaining_time_ss.str();

    // 打印进度信息
    std::cout << "\rTransfer ID: " << progress.transfer_id
              << " | Progress: " << std::fixed << std::setprecision(2) << percentage << "%"
              << " | Transferred: " << progress.bytes_transferred << " / " << progress.total_bytes << " bytes"
              << " | Speed: " << speed_str
              << " | ETA: " << remaining_time_str
              << std::flush;
}

// 更新进度信息
void ProgressDisplay::update_progress(const lansend::models::TransferProgress::TransferProgress& progress) {
    print_progress(progress);
}

// 清除进度信息
void ProgressDisplay::clear_progress() {
    std::cout << "\r" << std::string(120, ' ') << "\r" << std::flush;
}
