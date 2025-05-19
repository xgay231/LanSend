#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <network/http_server.hpp>

namespace lansend {

class CommonApiController {
public:
    CommonApiController(HttpServer& server);
    ~CommonApiController() = default;

private:
    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onPing(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onInfo(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> onConnect(
        const boost::beast::http::request<boost::beast::http::string_body>& req);

    void InstallRoutes(HttpServer& server);
};

} // namespace lansend