#include "logger.hpp"
#include <iostream>

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

void Logger::info(const std::string& message) {
    std::cout << "[INFO]: " << message << std::endl;
}

void Logger::warn(const std::string& message) {
    std::cout << "[WARN]: " << message << std::endl;
}

void Logger::error(const std::string& message) {
    std::cout << "[ERROR]: " << message << std::endl;
}
