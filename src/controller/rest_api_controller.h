#pragma once

#include "controller/receive_controller.h"
#include <api/http_server.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/string_body_fwd.hpp>

namespace lansend {

class RestApiController {
public:
    RestApiController(api::HttpServer& server);
    ~RestApiController() = default;

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> OnPing(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> OnInfo(
        const boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>& req);

private:
    void InstallRoutes();

    api::HttpServer& server_;
    ReceiveController receive_controller_;
};

} // namespace lansend