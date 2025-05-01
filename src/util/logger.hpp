#pragma once
#include <string>

class Logger {
public:
    static Logger& get_instance();

    // 添加以下方法以解决 "info" 成员缺失的问题
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
};
