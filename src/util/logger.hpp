#pragma once
#include <string>

class Logger {
public:
    static Logger& get_instance();

    // Logs an informational message to the appropriate output.
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
};
