#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <queue>
#include <memory>
#include <functional>
#include <string>

namespace openscad::ai {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

class HttpClient : public std::enable_shared_from_this<HttpClient>
{
public:
  using Callback = std::function<void(const std::string&, const std::string&)>;

  explicit HttpClient(net::io_context& ioc, ssl::context& ssl_ctx);

  void post(const std::string& url, const std::string& payload, Callback callback);

private:
  struct Request {
    std::string host;
    std::string port;
    std::string target;
    std::string payload;
    bool is_ssl;
    Callback callback;
  };

  void do_resolve();
  void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
  void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
  void on_handshake(beast::error_code ec);
  void on_write(beast::error_code ec, std::size_t bytes_transferred);
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void finish_request(const std::string& body, const std::string& error);

  net::io_context& ioc_;
  ssl::context& ssl_ctx_;
  tcp::resolver resolver_;

  // Beast streams – only one is active at a time
  std::unique_ptr<beast::tcp_stream> plain_stream_;
  std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> ssl_stream_;

  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  http::response<http::string_body> res_;

  std::queue<Request> requests_;
  Request current_request_;
};

}  // namespace openscad::ai
