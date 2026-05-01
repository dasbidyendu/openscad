#include "gui/ai/AIService.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "core/Settings.h"

AIService::AIService(QObject *parent) : QObject(parent), networkManager(new QNetworkAccessManager(this))
{
}

AIService::~AIService()
{
}

void AIService::sendMessage(const QString& text, const QJsonArray& history, const QString& systemContext)
{
  QString endpoint = QString::fromStdString(Settings::Settings::aiEndpoint.value());
  QString model = QString::fromStdString(Settings::Settings::aiModel.value());

  QNetworkRequest request{QUrl(endpoint)};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject payload;
  payload["model"] = model;
  payload["stream"] = false;  // Phase 1: Full response

  QJsonArray messages;

  if (!systemContext.isEmpty()) {
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemContext;
    messages.append(sysMsg);
  }

  for (const auto& msg : history) {
    messages.append(msg);
  }
  QJsonObject newMessage;
  newMessage["role"] = "user";
  newMessage["content"] = text;
  messages.append(newMessage);

  payload["messages"] = messages;
  payload["tools"] = getAvailableTools();

  QJsonDocument doc(payload);
  QNetworkReply *reply = networkManager->post(request, doc.toJson());
  connect(reply, &QNetworkReply::finished, this, &AIService::onReplyFinished);
}

void AIService::sendToolResponse(const QString& toolCallId, const QString& content,
                                 const QJsonArray& history, const QString& systemContext)
{
  QString endpoint = QString::fromStdString(Settings::Settings::aiEndpoint.value());
  QString model = QString::fromStdString(Settings::Settings::aiModel.value());

  QNetworkRequest request{QUrl(endpoint)};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject payload;
  payload["model"] = model;
  payload["stream"] = false;

  QJsonArray messages;
  if (!systemContext.isEmpty()) {
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemContext;
    messages.append(sysMsg);
  }

  for (const auto& msg : history) {
    messages.append(msg);
  }

  // OpenAI protocol requires the tool response to follow a tool_call message
  QJsonObject toolMsg;
  toolMsg["role"] = "tool";
  toolMsg["tool_call_id"] = toolCallId;
  toolMsg["content"] = content;
  messages.append(toolMsg);

  payload["messages"] = messages;
  payload["tools"] = getAvailableTools();

  QJsonDocument doc(payload);
  QNetworkReply *reply = networkManager->post(request, doc.toJson());
  connect(reply, &QNetworkReply::finished, this, &AIService::onReplyFinished);
}

void AIService::onReplyFinished()
{
  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply) return;

  if (reply->error() == QNetworkReply::NoError) {
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonArray choices = root["choices"].toArray();

    if (!choices.isEmpty()) {
      QJsonObject choice = choices[0].toObject();
      QJsonObject message = choice["message"].toObject();

      QString content = message["content"].toString();
      if (!content.isEmpty()) {
        emit responseReceived(content);
      }

      if (message.contains("tool_calls")) {
        QJsonArray toolCalls = message["tool_calls"].toArray();
        for (const auto& toolCallVal : toolCalls) {
          QJsonObject toolCall = toolCallVal.toObject();
          QString toolCallId = toolCall["id"].toString();
          QString toolName = toolCall["function"].toObject()["name"].toString();
          QJsonObject args =
            QJsonDocument::fromJson(toolCall["function"].toObject()["arguments"].toString().toUtf8())
              .object();
          emit toolCallReceived(toolCallId, toolName, args);
        }
      } else if (content.isEmpty()) {
        // Fallback: If no content AND no tool calls, emit a generic message to clear loading
        emit responseReceived("AI provided an empty response.");
      }
    } else {
      emit errorOccurred("AI returned an empty choice list.");
    }
  } else {
    emit errorOccurred(reply->errorString());
  }
  reply->deleteLater();
}

QJsonArray AIService::getAvailableTools() const
{
  QJsonArray tools;

  // set_editor_code
  QJsonObject setEditorCode;
  setEditorCode["type"] = "function";
  QJsonObject setEditorCodeFunc;
  setEditorCodeFunc["name"] = "set_editor_code";
  setEditorCodeFunc["description"] =
    "Replace the current editor content with the FULL corrected SCAD code. "
    "Use this for fixes, feature additions, or refactors. "
    "IMPORTANT: Always preserve the user's variables and module structure unless explicitly told "
    "otherwise.";
  QJsonObject setEditorCodeParams;
  setEditorCodeParams["type"] = "object";
  QJsonObject setEditorCodeProperties;
  QJsonObject codeProperty;
  codeProperty["type"] = "string";
  codeProperty["description"] = "The complete new SCAD script.";
  setEditorCodeProperties["code"] = codeProperty;
  setEditorCodeParams["properties"] = setEditorCodeProperties;
  setEditorCodeFunc["parameters"] = setEditorCodeParams;
  setEditorCode["function"] = setEditorCodeFunc;
  tools.append(setEditorCode);

  // trigger_preview
  QJsonObject triggerPreview;
  triggerPreview["type"] = "function";
  QJsonObject triggerPreviewFunc;
  triggerPreviewFunc["name"] = "trigger_preview";
  triggerPreviewFunc["description"] = "Triggers the F5 preview action in OpenSCAD.";
  QJsonObject triggerPreviewParams;
  triggerPreviewParams["type"] = "object";
  triggerPreviewParams["properties"] = QJsonObject();
  triggerPreviewFunc["parameters"] = triggerPreviewParams;
  triggerPreview["function"] = triggerPreviewFunc;
  tools.append(triggerPreview);

  // get_editor_code
  QJsonObject getCode;
  getCode["type"] = "function";
  QJsonObject getCodeFunc;
  getCodeFunc["name"] = "get_editor_code";
  getCodeFunc["description"] =
    "Retrieve the current SCAD code from the editor to see existing logic or syntax errors.";
  QJsonObject getCodeParams;
  getCodeParams["type"] = "object";
  getCodeParams["properties"] = QJsonObject();
  getCodeFunc["parameters"] = getCodeParams;
  getCode["function"] = getCodeFunc;
  tools.append(getCode);

  // get_console_output
  QJsonObject getLog;
  getLog["type"] = "function";
  QJsonObject getLogFunc;
  getLogFunc["name"] = "get_console_output";
  getLogFunc["description"] =
    "Retrieve recent console output. Use this if a preview fails to identify which line has a syntax "
    "error (e.g., missing semicolon).";
  QJsonObject getLogParams;
  getLogParams["type"] = "object";
  getLogParams["properties"] = QJsonObject();
  getLogFunc["parameters"] = getLogParams;
  getLog["function"] = getLogFunc;
  tools.append(getLog);

  return tools;
}

void AIService::setEndpoint(const QString& url)
{
  endpointUrl = url;
}
void AIService::setModel(const QString& model)
{
  modelName = model;
}
