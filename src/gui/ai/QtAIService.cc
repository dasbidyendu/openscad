#include "gui/ai/QtAIService.h"
#include "ai/AIService.h"
#include <QJsonDocument>
#include <QMetaObject>

using json = nlohmann::json;

namespace {
json qjsonToNlohmann(const QJsonArray& qarray)
{
  QJsonDocument doc(qarray);
  return json::parse(doc.toJson().constData());
}

json qjsonToNlohmann(const QJsonObject& qobj)
{
  QJsonDocument doc(qobj);
  return json::parse(doc.toJson().constData());
}

QJsonObject nlohmannToQJson(const json& j)
{
  return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).object();
}
}  // namespace

QtAIService::QtAIService(QObject *parent)
  : QObject(parent), coreService(std::make_unique<openscad::ai::AIService>())
{
}

QtAIService::~QtAIService()
{
}

void QtAIService::sendMessage(const QString& text, const QJsonArray& history,
                              const QString& systemContext)
{
  coreService->sendMessage(
    text.toStdString(), qjsonToNlohmann(history), systemContext.toStdString(),
    [this](const std::string& response) {
      QMetaObject::invokeMethod(
        this, [this, response]() { emit responseReceived(QString::fromStdString(response)); });
    },
    [this](const std::string& error) {
      QMetaObject::invokeMethod(this,
                                [this, error]() { emit errorOccurred(QString::fromStdString(error)); });
    },
    [this](const std::string& id, const std::string& name, const json& args) {
      QMetaObject::invokeMethod(this, [this, id, name, args]() {
        emit toolCallReceived(QString::fromStdString(id), QString::fromStdString(name),
                              nlohmannToQJson(args));
      });
    });
}

void QtAIService::sendToolResponse(const QString& toolCallId, const QString& content,
                                   const QJsonArray& history, const QString& systemContext)
{
  coreService->sendToolResponse(
    toolCallId.toStdString(), content.toStdString(), qjsonToNlohmann(history),
    systemContext.toStdString(),
    [this](const std::string& response) {
      QMetaObject::invokeMethod(
        this, [this, response]() { emit responseReceived(QString::fromStdString(response)); });
    },
    [this](const std::string& error) {
      QMetaObject::invokeMethod(this,
                                [this, error]() { emit errorOccurred(QString::fromStdString(error)); });
    },
    [this](const std::string& id, const std::string& name, const json& args) {
      QMetaObject::invokeMethod(this, [this, id, name, args]() {
        emit toolCallReceived(QString::fromStdString(id), QString::fromStdString(name),
                              nlohmannToQJson(args));
      });
    });
}
