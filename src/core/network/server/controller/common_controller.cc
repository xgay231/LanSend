#include <boost/beast/http/string_body_fwd.hpp>
#include <core/constant/route.h>
#include <core/model.h>
#include <core/network/server/controller/common_controller.h>
#include <core/network/server/http_server.h>
#include <core/util/config.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace net = boost::asio;
namespace http = boost::beast::http;
using json = nlohmann::json;

namespace lansend {

CommonController::CommonController(HttpServer& server) {
    InstallRoutes(server);
}

net::awaitable<http::response<http::string_body>> CommonController::onPing(
    const http::request<http::string_body>& req) {
    spdlog::debug("CommonController::OnPing");
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

net::awaitable<http::response<http::string_body>> CommonController::onConnect(
    const http::request<http::string_body>& req) {
    spdlog::debug("CommonController::OnConnect");
    json data = nlohmann::json::parse(req.body());
    std::string auth_code = data["auth_code"];
    models::DeviceInfo device_info = data["device_info"];
    if (settings.authCode != auth_code) {
        spdlog::info("Auth code mismatch, reject connection from {} ({}:{})",
                     device_info.hostname,
                     device_info.ip_address,
                     device_info.port);
        http::response<http::string_body> res{http::status::forbidden, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = R"({"success": false, "message": "Auth code mismatch"})";
        res.prepare_payload();
        co_return res;
    }
    spdlog::info("Connection accepted from {} ({}:{})",
                 device_info.hostname,
                 device_info.ip_address,
                 device_info.port);
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    res.body() = R"({"success": true, "message": "Connection accepted"})";
    res.prepare_payload();
    co_return res;
}

void CommonController::InstallRoutes(HttpServer& server) {
    server.AddRoute(ApiRoute::kPing.data(),
                    http::verb::get,
                    std::bind(&CommonController::onPing, this, std::placeholders::_1));
    server.AddRoute(ApiRoute::kConnect.data(),
                    http::verb::post,
                    std::bind(&CommonController::onConnect, this, std::placeholders::_1));
}

} // namespace lansend