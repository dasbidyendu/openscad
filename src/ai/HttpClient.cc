#include "ai/HttpClient.h"
#include <iostream>
#include <ostream>

namespace openscad::ai {

HttpClient::HttpClient(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx)
  : ioc_(ioc), ssl_ctx_(ssl_ctx), resolver_(ioc)
{
}

void HttpClient::post(const std::string& url, const std::string& payload, Callback callback)
{
  std::string protocol = "";
  std::string host, port, target = "/";

  std::string remaining = url;
  size_t proto_end = remaining.find("://");
  if (proto_end != std::string::npos) {
    protocol = remaining.substr(0, proto_end);
    remaining = remaining.substr(proto_end + 3);
  }

  size_t path_start = remaining.find("/");
  if (path_start != std::string::npos) {
    host = remaining.substr(0, path_start);
    target = remaining.substr(path_start);
  } else {
    host = remaining;
  }

  size_t port_start = host.find(":");
  if (port_start != std::string::npos) {
    port = host.substr(port_start + 1);
    host = host.substr(0, port_start);
  }

  // Smart protocol detection if missing
  if (protocol.empty()) {
    if (port == "443") {
      protocol = "https";
    } else if (host == "localhost" || host == "127.0.0.1" || port == "11434" || port == "80") {
      protocol = "http";
    } else {
      protocol = "https";  // Safe default for external domains
    }
  }

  if (port.empty()) {
    port = (protocol == "https") ? "443" : "80";
  }

  bool is_ssl = (protocol == "https");
  requests_.push({host, port, target, payload, is_ssl, callback});
  if (requests_.size() == 1) {
    resolve();
  }
}

void HttpClient::resolve()
{
  response_data_.clear();
  current_request_ = requests_.front();
  resolver_.async_resolve(
    current_request_.host, current_request_.port,
    [self = shared_from_this()](boost::system::error_code ec,
                                boost::asio::ip::tcp::resolver::results_type results) {
      if (!ec) self->connect(results);
      else self->current_request_.callback("", "Resolve error: " + ec.message());
    });
}

void HttpClient::connect(boost::asio::ip::tcp::resolver::results_type results)
{
  ssl_stream_.reset();
  socket_.reset();
  socket_ = std::make_unique<boost::asio::ip::tcp::socket>(ioc_);

  boost::asio::async_connect(
    *socket_, results,
    [self = shared_from_this()](boost::system::error_code ec,
                                boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint) {
      if (!ec) {
        if (self->current_request_.is_ssl) {
          self->handshake();
        } else {
          self->write();
        }
      } else {
        self->current_request_.callback("", "Connect error: " + ec.message());
      }
    });
}

void HttpClient::handshake()
{
  ssl_stream_ =
    std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>>(*socket_, ssl_ctx_);

  if (!SSL_set_tlsext_host_name(ssl_stream_->native_handle(), current_request_.host.c_str())) {
    current_request_.callback("", "SSL SNI error");
    return;
  }

  ssl_stream_->async_handshake(
    boost::asio::ssl::stream_base::client, [self = shared_from_this()](boost::system::error_code ec) {
      if (!ec) self->write();
      else self->current_request_.callback("", "Handshake error: " + ec.message());
    });
}

void HttpClient::write()
{
  std::ostream request_stream(&response_buf_);
  request_stream << "POST " << current_request_.target << " HTTP/1.1\r\n";
  request_stream << "Host: " << current_request_.host << "\r\n";
  request_stream << "Content-Type: application/json\r\n";
  request_stream << "Content-Length: " << current_request_.payload.length() << "\r\n";
  request_stream << "Connection: close\r\n\r\n";
  request_stream << current_request_.payload;

  auto on_write = [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
    if (!ec) self->read_header();
    else self->current_request_.callback("", "Write error: " + ec.message());
  };

  if (current_request_.is_ssl) {
    boost::asio::async_write(*ssl_stream_, response_buf_, on_write);
  } else {
    boost::asio::async_write(*socket_, response_buf_, on_write);
  }
}

void HttpClient::read_header()
{
  response_buf_.consume(response_buf_.size());

  auto on_read = [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
    if (!ec) {
      std::istream response_stream(&self->response_buf_);
      std::string status_line;
      std::getline(response_stream, status_line);
      if (status_line.find(" 200 ") == std::string::npos) {
        self->current_request_.callback("", "HTTP Error: " + status_line);
        return;
      }
      std::string header;
      std::size_t content_length = 0;
      bool is_chunked = false;
      while (std::getline(response_stream, header) && header != "\r") {
        if (header.find("Content-Length:") == 0) {
          content_length = std::stoul(header.substr(15));
        } else if (header.find("Transfer-Encoding: chunked") != std::string::npos) {
          is_chunked = true;
        }
      }

      if (is_chunked) {
        self->read_chunks();
      } else {
        self->read_body(content_length);
      }
    } else {
      self->current_request_.callback("", "Read header error: " + ec.message());
    }
  };

  if (current_request_.is_ssl) {
    boost::asio::async_read_until(*ssl_stream_, response_buf_, "\r\n\r\n", on_read);
  } else {
    boost::asio::async_read_until(*socket_, response_buf_, "\r\n\r\n", on_read);
  }
}

void HttpClient::read_chunks()
{
  auto on_size = [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
    if (!ec) {
      std::istream response_stream(&self->response_buf_);
      std::string line;
      std::getline(response_stream, line);
      if (!line.empty() && line.back() == '\r') line.pop_back();

      try {
        std::size_t chunk_size = std::stoul(line, nullptr, 16);
        if (chunk_size == 0) {
          self->current_request_.callback(self->response_data_, "");
          self->response_data_.clear();
          self->requests_.pop();
          if (!self->requests_.empty()) self->resolve();
        } else {
          self->read_chunk_data(chunk_size);
        }
      } catch (...) {
        self->current_request_.callback("", "Invalid chunk size: " + line);
      }
    } else {
      self->current_request_.callback("", "Read chunk size error: " + ec.message());
    }
  };

  if (current_request_.is_ssl) {
    boost::asio::async_read_until(*ssl_stream_, response_buf_, "\r\n", on_size);
  } else {
    boost::asio::async_read_until(*socket_, response_buf_, "\r\n", on_size);
  }
}

void HttpClient::read_chunk_data(std::size_t chunk_size)
{
  std::size_t already_read = response_buf_.size();
  auto on_data = [self = shared_from_this(), chunk_size](boost::system::error_code ec,
                                                         std::size_t bytes) {
    if (!ec) {
      std::istream response_stream(&self->response_buf_);
      std::string chunk(chunk_size, ' ');
      response_stream.read(&chunk[0], chunk_size);
      self->response_data_ += chunk;

      auto on_trailer = [self](boost::system::error_code ec, std::size_t bytes) {
        if (!ec) {
          self->response_buf_.consume(2);
          self->read_chunks();
        } else {
          self->current_request_.callback("", "Read chunk trailer error: " + ec.message());
        }
      };

      if (self->current_request_.is_ssl) {
        boost::asio::async_read(*self->ssl_stream_, self->response_buf_,
                                boost::asio::transfer_exactly(2), on_trailer);
      } else {
        boost::asio::async_read(*self->socket_, self->response_buf_, boost::asio::transfer_exactly(2),
                                on_trailer);
      }
    } else {
      self->current_request_.callback("", "Read chunk data error: " + ec.message());
    }
  };

  if (already_read >= chunk_size) {
    on_data(boost::system::error_code(), 0);
  } else {
    if (current_request_.is_ssl) {
      boost::asio::async_read(*ssl_stream_, response_buf_,
                              boost::asio::transfer_exactly(chunk_size - already_read), on_data);
    } else {
      boost::asio::async_read(*socket_, response_buf_,
                              boost::asio::transfer_exactly(chunk_size - already_read), on_data);
    }
  }
}

void HttpClient::read_body(std::size_t content_length)
{
  auto on_read_done = [self = shared_from_this(), content_length](boost::system::error_code ec,
                                                                  std::size_t bytes) {
    if (!ec || ec == boost::asio::error::eof) {
      std::istream response_stream(&self->response_buf_);
      std::string body;
      if (content_length > 0) {
        body.resize(content_length);
        response_stream.read(&body[0], content_length);
      } else {
        // Read everything until EOF
        std::stringstream ss;
        ss << &self->response_buf_;
        body = ss.str();
      }
      self->current_request_.callback(body, "");
      self->requests_.pop();
      if (!self->requests_.empty()) self->resolve();
    } else {
      self->current_request_.callback("", "Read body error: " + ec.message());
    }
  };

  if (content_length > 0) {
    std::size_t already_read = response_buf_.size();
    if (already_read >= content_length) {
      on_read_done(boost::system::error_code(), 0);
    } else {
      if (current_request_.is_ssl) {
        boost::asio::async_read(*ssl_stream_, response_buf_,
                                boost::asio::transfer_exactly(content_length - already_read),
                                on_read_done);
      } else {
        boost::asio::async_read(*socket_, response_buf_,
                                boost::asio::transfer_exactly(content_length - already_read),
                                on_read_done);
      }
    }
  } else {
    // No content length, read until EOF
    if (current_request_.is_ssl) {
      boost::asio::async_read(*ssl_stream_, response_buf_, boost::asio::transfer_at_least(1),
                              on_read_done);
    } else {
      boost::asio::async_read(*socket_, response_buf_, boost::asio::transfer_at_least(1), on_read_done);
    }
  }
}

}  // namespace openscad::ai
