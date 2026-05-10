#pragma once

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

namespace openscad::ai {
class AIService;
}

class QtAIService : public QObject
{
  Q_OBJECT

public:
  explicit QtAIService(QObject *parent = nullptr);
  virtual ~QtAIService();

  void sendMessage(const QString& text, const QJsonArray& history, const QString& systemContext = "");
  void sendToolResponse(const QString& toolCallId, const QString& content, const QJsonArray& history,
                        const QString& systemContext = "");

signals:
  void responseReceived(const QString& text);
  void errorOccurred(const QString& error);
  void toolCallReceived(const QString& toolCallId, const QString& toolName,
                        const QJsonObject& arguments);

private:
  std::unique_ptr<openscad::ai::AIService> coreService;
};
