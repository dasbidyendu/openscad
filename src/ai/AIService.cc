#include "ai/AIService.h"
#include "ai/HttpClient.h"
#include "core/Settings.h"
#include <iostream>

using json = nlohmann::json;

namespace openscad::ai {

AIService::AIService() : ssl_ctx_(boost::asio::ssl::context::sslv23_client)
{
  ssl_ctx_.set_default_verify_paths();
  client_ = std::make_shared<HttpClient>(ioc_, ssl_ctx_);

  work_guard_ =
    std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
      ioc_.get_executor());
  worker_thread_ = std::thread([this]() { ioc_.run(); });
}

AIService::~AIService()
{
  work_guard_.reset();
  ioc_.stop();
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

void AIService::sendMessage(const std::string& text, const json& history,
                            const std::string& systemContext, ResponseCallback onResponse,
                            ErrorCallback onError, ToolCallCallback onToolCall)
{
  json payload;
  payload["model"] = Settings::Settings::aiModel.value();
  payload["stream"] = false;
  lastAssistantMessage_ = nlohmann::json();  // Clear on new user message

  json messages = json::array();
  if (!systemContext.empty()) {
    messages.push_back({{"role", "system"}, {"content", systemContext}});
  }

  for (const auto& msg : history) {
    messages.push_back(msg);
  }

  messages.push_back({{"role", "user"}, {"content", text}});
  payload["messages"] = messages;
  payload["tools"] = getAvailableTools();

  sendRequest(payload, onResponse, onError, onToolCall);
}

void AIService::sendToolResponse(const std::string& toolCallId, const std::string& content,
                                 const json& history, const std::string& systemContext,
                                 ResponseCallback onResponse, ErrorCallback onError,
                                 ToolCallCallback onToolCall)
{
  json payload;
  payload["model"] = Settings::Settings::aiModel.value();
  payload["stream"] = false;

  json messages = json::array();
  if (!systemContext.empty()) {
    messages.push_back({{"role", "system"}, {"content", systemContext}});
  }

  for (const auto& msg : history) {
    messages.push_back(msg);
  }

  if (!lastAssistantMessage_.is_null()) {
    messages.push_back(lastAssistantMessage_);
  }

  messages.push_back({{"role", "tool"}, {"tool_call_id", toolCallId}, {"content", content}});

  payload["messages"] = messages;
  payload["tools"] = getAvailableTools();

  sendRequest(payload, onResponse, onError, onToolCall);
}

void AIService::sendRequest(const json& payload, ResponseCallback onResponse, ErrorCallback onError,
                            ToolCallCallback onToolCall)
{
  std::string endpoint = Settings::Settings::aiEndpoint.value();
  std::string body = payload.dump();

  client_->post(
    endpoint, body,
    [this, onResponse, onError, onToolCall](const std::string& response, const std::string& error) {
      if (!error.empty()) {
        onError(error);
        return;
      }

      try {
        auto root = json::parse(response);
        if (root.contains("choices") && !root["choices"].empty()) {
          auto message = root["choices"][0]["message"];
          std::string content = message.value("content", "");

          if (!content.empty()) {
            onResponse(content);
          }

          if (message.contains("tool_calls")) {
            lastAssistantMessage_ = message;  // Save for next tool response
            for (const auto& toolCall : message["tool_calls"]) {
              std::string id = toolCall["id"];
              std::string name = toolCall["function"]["name"];
              json args = toolCall["function"]["arguments"];
              if (args.is_string()) {
                args = json::parse(args.get<std::string>());
              }
              onToolCall(id, name, args);
            }
          } else if (content.empty()) {
            onResponse("AI provided an empty response.");
          }
        } else {
          onError("AI returned an empty choice list. Raw response: " + response);
        }
      } catch (const std::exception& e) {
        onError(std::string("JSON parse error: ") + e.what() + ". Raw response: " + response);
      }
    });
}

json AIService::getAvailableTools() const
{
  json tools = json::array();

  // set_editor_code
  tools.push_back(
    {{"type", "function"},
     {"function",
      {{"name", "set_editor_code"},
       {"description", "Replace the current editor content with the FULL corrected SCAD code."},
       {"parameters",
        {{"type", "object"},
         {"properties",
          {{"code", {{"type", "string"}, {"description", "The complete new SCAD script."}}}}}}}}}});

  // trigger_preview
  tools.push_back({{"type", "function"},
                   {"function",
                    {{"name", "trigger_preview"},
                     {"description", "Triggers the F5 preview action in OpenSCAD."},
                     {"parameters", {{"type", "object"}, {"properties", json::object()}}}}}});

  // get_editor_code
  tools.push_back({{"type", "function"},
                   {"function",
                    {{"name", "get_editor_code"},
                     {"description", "Retrieve the current SCAD code from the editor."},
                     {"parameters", {{"type", "object"}, {"properties", json::object()}}}}}});

  // get_console_output
  tools.push_back({{"type", "function"},
                   {"function",
                    {{"name", "get_console_output"},
                     {"description", "Retrieve recent console output."},
                     {"parameters", {{"type", "object"}, {"properties", json::object()}}}}}});

  return tools;
}

}  // namespace openscad::ai
