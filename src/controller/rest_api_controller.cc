#include "rest_api_controller.h"
#include "controller/receive_controller.h"
#include "network/http_server.hpp"
#include <boost/beast/http/string_body_fwd.hpp>
#include <constants/route.hpp>
#include <models/device_info.h>
#include <nlohmann/json.hpp>
#include <utils/config.hpp>

namespace net = boost::asio;
namespace http = boost::beast::http;
using json = nlohmann::json;

namespace lansend {

RestApiController::RestApiController(HttpServer& server)
    : server_(server)
    , receive_controller_(server) {
    InstallRoutes();
    receive_controller_.SetSaveDirectory(settings.saveDir);
}

net::awaitable<http::response<http::string_body>> RestApiController::OnPing(
    const http::request<http::string_body>& req) {
    spdlog::debug("RestApiController::OnPing");
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

    if (req[http::field::connection] == "close") {
        res.set(http::field::connection, "close");
    }

    json j = {{"status", "ok"}, {"message", "server is running"}};
    res.body() = j.dump();
    res.prepare_payload();
    co_return res;
}

net::awaitable<http::response<http::string_body>> RestApiController::OnInfo(
    const http::request<http::string_body>& req) {
    spdlog::debug("RestApiController::OnInfo");
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    if (req[http::field::connection] == "close") {
        res.set(http::field::connection, "close");
    }

    auto& device_info = models::DeviceInfo::LocalDeviceInfo();

    json response = device_info;
    res.body() = response.dump();
    res.prepare_payload();
    co_return res;
}

void RestApiController::InstallRoutes() {
    server_.AddRoute(ApiRoute::kPing.data(),
                     http::verb::get,
                     std::bind(&RestApiController::OnPing, this, std::placeholders::_1));
    server_.AddRoute(ApiRoute::kInfo.data(),
                     http::verb::get,
                     std::bind(&RestApiController::OnInfo, this, std::placeholders::_1));
}

} // namespace lansend