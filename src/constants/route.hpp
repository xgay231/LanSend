#pragma once

#include <string_view>

namespace lansend {

class ApiRoute {
public:
    static constexpr std::string_view kPing = "/ping";
    static constexpr std::string_view kInfo = "/info";
    static constexpr std::string_view kSendRequest = "/send-request";
    static constexpr std::string_view kPrepareSend = "/prepare-send";
    static constexpr std::string_view kSendChunk = "/send-chunk";
    static constexpr std::string_view kVerifyAndComplete = "/verify-and-complete";
    static constexpr std::string_view kCancelSend = "/cancel-send";
    static constexpr std::string_view kCancelReceive = "/cancel-receive";
};

} // namespace lansend