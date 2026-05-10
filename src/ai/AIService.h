#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <ext/json/json.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace openscad::ai {

class HttpClient;

class AIService
{
public:
  using ResponseCallback = std::function<void(const std::string&)>;
  using ErrorCallback = std::function<void(const std::string&)>;
  using ToolCallCallback =
    std::function<void(const std::string& id, const std::string& name, const nlohmann::json& args)>;

  AIService();
  ~AIService();

  void sendMessage(const std::string& text, const nlohmann::json& history,
                   const std::string& systemContext, ResponseCallback onResponse, ErrorCallback onError,
                   ToolCallCallback onToolCall);

  void sendToolResponse(const std::string& toolCallId, const std::string& content,
                        const nlohmann::json& history, const std::string& systemContext,
                        ResponseCallback onResponse, ErrorCallback onError, ToolCallCallback onToolCall);

  nlohmann::json getAvailableTools() const;

private:
  void sendRequest(const nlohmann::json& payload, ResponseCallback onResponse, ErrorCallback onError,
                   ToolCallCallback onToolCall);

  boost::asio::io_context ioc_;
  boost::asio::ssl::context ssl_ctx_;
  std::shared_ptr<HttpClient> client_;
  std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
  std::thread worker_thread_;
  nlohmann::json lastAssistantMessage_;
};

}  // namespace openscad::ai
