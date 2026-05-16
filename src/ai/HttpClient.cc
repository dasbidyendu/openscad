#include "ai/HttpClient.h"

namespace openscad::ai {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

HttpClient::HttpClient(net::io_context& ioc, ssl::context& ssl_ctx)
  : ioc_(ioc), ssl_ctx_(ssl_ctx), resolver_(ioc)
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void HttpClient::post(const std::string& url, const std::string& payload, Callback callback)
{
  // ---- Parse the URL -------------------------------------------------------
  std::string protocol;
  std::string host;
  std::string port;
  std::string target = "/";

  std::string remaining = url;
  size_t proto_end = remaining.find("://");
  if (proto_end != std::string::npos) {
    protocol = remaining.substr(0, proto_end);
    remaining = remaining.substr(proto_end + 3);
  }

  size_t path_start = remaining.find('/');
  if (path_start != std::string::npos) {
    host = remaining.substr(0, path_start);
    target = remaining.substr(path_start);
  } else {
    host = remaining;
  }

  size_t port_start = host.find(':');
  if (port_start != std::string::npos) {
    port = host.substr(port_start + 1);
    host = host.substr(0, port_start);
  }

  if (protocol.empty()) {
    if (port == "443") {
      protocol = "https";
    } else if (host == "localhost" || host == "127.0.0.1" || port == "11434" || port == "80") {
      protocol = "http";
    } else {
      protocol = "https";
    }
  }

  if (port.empty()) {
    port = (protocol == "https") ? "443" : "80";
  }

  bool is_ssl = (protocol == "https");

  // ---- Enqueue and start ---------------------------------------------------
  requests_.push({host, port, target, payload, is_ssl, std::move(callback)});
  if (requests_.size() == 1) {
    do_resolve();
  }
}

// ---------------------------------------------------------------------------
// Private pipeline
// ---------------------------------------------------------------------------

void HttpClient::do_resolve()
{
  current_request_ = requests_.front();

  // Reset per-request state
  buffer_.clear();
  res_ = {};

  // Build the Beast HTTP request
  req_.method(http::verb::post);
  req_.target(current_request_.target);
  req_.version(11);  // HTTP/1.1
  req_.set(http::field::host, current_request_.host);
  req_.set(http::field::content_type, "application/json");
  req_.set(http::field::connection, "close");
  req_.body() = current_request_.payload;
  req_.prepare_payload();  // sets Content-Length automatically

  resolver_.async_resolve(
    current_request_.host, current_request_.port,
    [self = shared_from_this()](beast::error_code ec, tcp::resolver::results_type results) {
      self->on_resolve(ec, std::move(results));
    });
}

void HttpClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
{
  if (ec) {
    finish_request("", "Resolve error: " + ec.message());
    return;
  }

  if (current_request_.is_ssl) {
    ssl_stream_ =
      std::make_unique<beast::ssl_stream<beast::tcp_stream>>(net::make_strand(ioc_), ssl_ctx_);

    // Set SNI hostname
    if (!SSL_set_tlsext_host_name(ssl_stream_->native_handle(), current_request_.host.c_str())) {
      beast::error_code sniec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      finish_request("", "SSL SNI error: " + sniec.message());
      return;
    }

    beast::get_lowest_layer(*ssl_stream_)
      .async_connect(results, [self = shared_from_this()](
                                beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        self->on_connect(ec, ep);
      });
  } else {
    plain_stream_ = std::make_unique<beast::tcp_stream>(net::make_strand(ioc_));
    plain_stream_->async_connect(
      results,
      [self = shared_from_this()](beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        self->on_connect(ec, ep);
      });
  }
}

void HttpClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
{
  if (ec) {
    finish_request("", "Connect error: " + ec.message());
    return;
  }

  if (current_request_.is_ssl) {
    ssl_stream_->async_handshake(
      ssl::stream_base::client,
      [self = shared_from_this()](beast::error_code ec) { self->on_handshake(ec); });
  } else {
    // Plain – go straight to writing
    http::async_write(*plain_stream_, req_,
                      [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                        self->on_write(ec, bytes);
                      });
  }
}

void HttpClient::on_handshake(beast::error_code ec)
{
  if (ec) {
    finish_request("", "Handshake error: " + ec.message());
    return;
  }

  http::async_write(
    *ssl_stream_, req_,
    [self = shared_from_this()](beast::error_code ec, std::size_t bytes) { self->on_write(ec, bytes); });
}

void HttpClient::on_write(beast::error_code ec, std::size_t /*bytes_transferred*/)
{
  if (ec) {
    finish_request("", "Write error: " + ec.message());
    return;
  }

  // Read the full response. Beast handles chunked transfer encoding for us.
  if (current_request_.is_ssl) {
    http::async_read(*ssl_stream_, buffer_, res_,
                     [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                       self->on_read(ec, bytes);
                     });
  } else {
    http::async_read(*plain_stream_, buffer_, res_,
                     [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                       self->on_read(ec, bytes);
                     });
  }
}

void HttpClient::on_read(beast::error_code ec, std::size_t /*bytes_transferred*/)
{
  if (ec && ec != http::error::end_of_stream) {
    finish_request("", "Read error: " + ec.message());
    return;
  }

  unsigned int status = res_.result_int();
  if (status != 200) {
    finish_request("", "HTTP error " + std::to_string(status) + ": " + std::string(res_.reason()));
    return;
  }

  finish_request(res_.body(), "");

  // Graceful shutdown (best-effort)
  if (current_request_.is_ssl) {
    ssl_stream_->async_shutdown(
      [self = shared_from_this()](beast::error_code) { /* ignore shutdown errors */ });
  } else {
    beast::error_code shutdown_ec;
    plain_stream_->socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
  }
}

void HttpClient::finish_request(const std::string& body, const std::string& error)
{
  auto cb = current_request_.callback;
  requests_.pop();

  cb(body, error);

  if (!requests_.empty()) {
    do_resolve();
  }
}

}  // namespace openscad::ai
