#pragma once

#include <boost/utility/string_view.hpp>
#include <format>
#include <string>
#include <string_view>

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
