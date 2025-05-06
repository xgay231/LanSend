#pragma once

#include <boost/utility/string_view.hpp>
#include <format>
#include <string>
#include <string_view>

namespace std {
template<typename CharT>
struct formatter<boost::basic_string_view<CharT>, CharT>
    : formatter<std::basic_string_view<CharT>, CharT> {
    template<typename FormatContext>
    auto format(boost::basic_string_view<CharT> bsv, FormatContext& ctx) const {
        return formatter<std::basic_string_view<CharT>,
                         CharT>::format(std::basic_string_view<CharT>(bsv.data(), bsv.size()), ctx);
    }
};
} // namespace std
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
