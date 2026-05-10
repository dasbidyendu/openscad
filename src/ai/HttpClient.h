#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <functional>
#include <memory>
#include <queue>

namespace openscad::ai {

class HttpClient : public std::enable_shared_from_this<HttpClient>
{
public:
  using Callback = std::function<void(const std::string&, const std::string&)>;

  explicit HttpClient(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx);

  void post(const std::string& url, const std::string& payload, Callback callback);

private:
  void resolve();
  void on_resolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
  void connect(boost::asio::ip::tcp::resolver::results_type results);
  void handshake();
  void write();
  void read_header();
  void read_body(std::size_t content_length);
  void read_chunks();
  void read_chunk_data(std::size_t chunk_size);

  struct Request {
    std::string host;
    std::string port;
    std::string target;
    std::string payload;
    bool is_ssl;
    Callback callback;
  };

  boost::asio::io_context& ioc_;
  boost::asio::ssl::context& ssl_ctx_;
  boost::asio::ip::tcp::resolver resolver_;

  std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
  std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>> ssl_stream_;

  std::queue<Request> requests_;
  Request current_request_;
  boost::asio::streambuf response_buf_;
  std::string response_data_;
};

}  // namespace openscad::ai
