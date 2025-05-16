#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace lansend {

class RestApiController;

using HttpRequest = boost::beast::http::request<boost::beast::http::vector_body<std::uint8_t>>;
using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;
using HttpChunkResponse = boost::beast::http::response<boost::beast::http::vector_body<std::uint8_t>>;
using AnyResponse = std::variant<HttpResponse, HttpChunkResponse>;

// 路由处理器函数类型
// using RouteHandler = std::function<boost::asio::awaitable<HttpResponse>(HttpRequest&&)>;
using RouteHandler = std::function<boost::asio::awaitable<AnyResponse>(HttpRequest&&)>;

// 路由信息结构体
struct RouteInfo {
    boost::beast::http::verb method;
    RouteHandler handler;
};

//HTTPS 服务器类
class HttpServer {
public:
    // 构造函数
    HttpServer(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context);

    // 析构函数
    ~HttpServer();

    // 添加路由
    void add_route(const std::string& path, boost::beast::http::verb method, RouteHandler handler);

    // 启动服务器
    void start(uint16_t port);

    // 停止服务器
    void stop();

private:
    // 接受连接
    boost::asio::awaitable<void> accept_connections();

    // 处理连接
    boost::asio::awaitable<void> handle_connection(
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream);

    // 处理请求
    boost::asio::awaitable<AnyResponse> handle_request(HttpRequest&& request);

    boost::asio::io_context& io_context_;
    boost::asio::ssl::context& ssl_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    bool running_;
    std::map<std::string, RouteInfo> routes_;
    std::unique_ptr<RestApiController> controller_;
};

} // namespace lansend
