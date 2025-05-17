
#include "http_server.hpp"
#include "controller/rest_api_controller.h"
#include "spdlog/spdlog.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <constants/transfer.h>

namespace lansend {

// Introduce awaitable operators for convenience.
using namespace boost::asio::experimental::awaitable_operators;

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

HttpServer::HttpServer(net::io_context& io_context, ssl::context& ssl_context)
    : io_context_(io_context)
    , ssl_context_(ssl_context)
    , acceptor_(io_context)
    , running_(false) {
    controller_ = std::make_unique<RestApiController>(*this);
    spdlog::info("HttpServer created.");
}

HttpServer::~HttpServer() {
    if (running_) {
        stop();
    }
    spdlog::info("HttpServer destroyed.");
}

void HttpServer::AddRoute(const std::string& path,
                          http::verb method,
                          BinaryRequestHandler&& handler) {
    routes_[path] = {method, RequestType::kBinary, std::move(handler)};
    spdlog::info(std::format("Added route: {} {}", std::string(http::to_string(method)), path));
}

void HttpServer::AddRoute(const std::string& path,
                          boost::beast::http::verb method,
                          StringRequestHandler&& handler) {
    routes_[path] = {method, RequestType::kString, std::move(handler)};
    spdlog::info(std::format("Added route: {} {}", std::string(http::to_string(method)), path));
}

void HttpServer::start(uint16_t port) {
    if (running_) {
        spdlog::warn("Server is already running.");
        return;
    }

    try {
        tcp::endpoint endpoint(tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
        running_ = true;
        spdlog::info(std::format("HTTPS Server started on port {}", port));

        net::co_spawn(io_context_, accept_connections(), net::detached);

    } catch (const std::exception& e) {
        spdlog::error(std::format("Failed to start server on port {}: {}", port, e.what()));
        running_ = false;
        if (acceptor_.is_open()) {
            acceptor_.close();
        }
    }
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    acceptor_.cancel();
    if (acceptor_.is_open()) {
        acceptor_.close();
    }
    spdlog::info("HTTPS Server stopped.");
}

boost::asio::awaitable<void> HttpServer::accept_connections() {
    while (running_) {
        try {
            tcp::socket socket = co_await acceptor_.async_accept();
            spdlog::info(std::format("Accepted connection from: {}",
                                     std::string(socket.remote_endpoint().address().to_string())));

            beast::ssl_stream<beast::tcp_stream> stream(beast::tcp_stream(std::move(socket)),
                                                        ssl_context_);

            net::co_spawn(io_context_, handle_connection(std::move(stream)), net::detached);
        } catch (const boost::system::system_error& e) {
            if (e.code() == net::error::operation_aborted) {
                spdlog::info("Accept operation cancelled.");
                break;
            } else {
                spdlog::error(std::format("Error accepting connection: {}", e.what()));
            }
        } catch (const std::exception& e) {
            spdlog::error(std::format("Unexpected error during accept: {}", e.what()));
        }
    }
    spdlog::info("Stopped accepting connections.");
}

boost::asio::awaitable<void> HttpServer::handle_connection(ssl::stream<beast::tcp_stream> stream) {
    try {
        auto& socket = stream.next_layer().socket();
        auto endpoint = socket.remote_endpoint();
        spdlog::info("New connection from: {}:{}", endpoint.address().to_string(), endpoint.port());

        co_await stream.async_handshake(ssl::stream_base::server, net::use_awaitable);
        spdlog::debug("SSL handshake completed successfully");

        beast::flat_buffer buffer;
        bool keep_alive = true;

        while (keep_alive) {
            try {
                beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

                http::request_parser<http::vector_body<uint8_t>> parser;
                parser.body_limit(transfer::kMaxChunkSize + 8);

                spdlog::debug("Waiting for client request...");
                co_await http::async_read(stream, buffer, parser);
                auto req = parser.release();

                spdlog::info("Received {} request for {}", req.method_string(), req.target());

                keep_alive = req.keep_alive();

                HttpResponse res = co_await handle_request(std::move(req));

                co_await http::async_write(stream, res);

                if (!keep_alive) {
                    spdlog::debug("Connection: close requested, ending session");
                    break;
                }
            } catch (const boost::system::system_error& e) {
                if (e.code() == boost::beast::error::timeout || e.code() == boost::asio::error::eof
                    || e.code() == boost::asio::error::operation_aborted
                    || e.code() == boost::beast::http::error::end_of_stream) {
                    spdlog::debug("Connection closed by peer: {}", e.code().message());
                    break;
                }
                throw e;
            }
        }

        spdlog::debug("Closing connection gracefully");
        beast::error_code ec;
        stream.shutdown(ec);
        if (ec && ec != net::error::eof) {
            spdlog::debug("Shutdown notice: {}", ec.message());
        }
    } catch (const std::exception& e) {
        std::string error_msg = e.what();
        if (error_msg.find("end of stream") != std::string::npos
            || error_msg.find("stream truncated") != std::string::npos
            || error_msg.find("operation canceled") != std::string::npos) {
            spdlog::debug("Session ended: {}", error_msg);
        } else {
            spdlog::error("Session error: {}", error_msg);
        }
    }
    spdlog::info("Connection handling finished.");
}

boost::asio::awaitable<HttpResponse> HttpServer::handle_request(HttpRequest&& req) {
    HttpResponse res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");

    std::string path(req.target());
    auto it = routes_.find(path);

    if (it == routes_.end()) {
        spdlog::warn(std::format("Route not found: {}", path));
        http::response<http::string_body> not_found_response;
        not_found_response.version(req.version());
        not_found_response.keep_alive(req.keep_alive());
        not_found_response.result(http::status::not_found);
        not_found_response.set(http::field::content_type, "application/json");
        not_found_response.body() = "{\"error\": \"Not Found\"}";
        not_found_response.prepare_payload();

        res = std::move(not_found_response);

        co_return res;
    }

    const auto& route_info = it->second;
    if (route_info.method != req.method()) {
        spdlog::warn(std::format("Method not allowed for route {}: requested {}, expected {}",
                                 path,
                                 std::string(http::to_string(req.method())),
                                 std::string(http::to_string(route_info.method))));
        http::response<http::string_body> error_res{http::status::method_not_allowed, req.version()};
        error_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        error_res.set(http::field::content_type, "application/json");
        error_res.set(http::field::allow, std::string(http::to_string(route_info.method)));
        error_res.keep_alive(req.keep_alive());
        error_res.body() = R"({\"error\": \"Method Not Allowed\"})";
        error_res.prepare_payload();
        res = std::move(error_res);
        co_return res;
    }

    auto request_version = req.version();
    bool request_keep_alive = req.keep_alive();

    try {
        if (route_info.type == RequestType::kString) {
            auto handler = std::get<StringRequestHandler>(route_info.handler);
            res = co_await handler(BinaryToStringRequest(req));
        } else {
            auto handler = std::get<BinaryRequestHandler>(route_info.handler);
            res = co_await handler(std::move(req));
        }
    } catch (const std::exception& e) {
        spdlog::error(std::format("Error executing handler for {}: {}", path, e.what()));

        http::response<http::string_body> error_res{http::status::internal_server_error,
                                                    request_version};
        error_res.keep_alive(request_keep_alive);
        error_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        error_res.set(http::field::content_type, "application/json");
        error_res.body() = R"({\"error\": \"Internal Server Error\"})";
        error_res.prepare_payload();

        res = std::move(error_res);
    }

    co_return res;
}

StringRequest HttpServer::BinaryToStringRequest(const BinaryRequest& req) {
    http::request<http::string_body> string_req;
    string_req.method(req.method());
    string_req.target(req.target());
    string_req.version(req.version());

    for (auto const& field : req)
        string_req.set(field.name(), field.value());

    std::string body_str(reinterpret_cast<const char*>(req.body().data()), req.body().size());
    string_req.body() = std::move(body_str);
    string_req.prepare_payload();

    return string_req;
}

} // namespace lansend