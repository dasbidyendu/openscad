#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class AIService : public QObject
{
  Q_OBJECT

public:
  explicit AIService(QObject *parent = nullptr);
  virtual ~AIService();

  void sendMessage(const QString& text, const QJsonArray& history, const QString& systemContext = "");
  void sendToolResponse(const QString& toolCallId, const QString& content, const QJsonArray& history,
                        const QString& systemContext = "");
  void setEndpoint(const QString& url);
  void setModel(const QString& model);

  QJsonArray getAvailableTools() const;

signals:
  void responseReceived(const QString& text);
  void errorOccurred(const QString& error);
  void toolCallReceived(const QString& toolCallId, const QString& toolName,
                        const QJsonObject& arguments);

private slots:
  void onReplyFinished();

private:
  QNetworkAccessManager *networkManager;
  QString endpointUrl;
  QString modelName;
};
