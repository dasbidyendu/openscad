#pragma once

#include <QAbstractListModel>
#include <QString>
#include <vector>

struct ChatMessage {
  QString text;
  bool isUser;
  bool isLoading = false;
};

class ChatModel : public QAbstractListModel
{
  Q_OBJECT

public:
  enum ChatRoles { TextRole = Qt::UserRole + 1, IsUserRole, IsLoadingRole };

  ChatModel(QObject *parent = nullptr);

  void addMessage(const QString& text, bool isUser);
  void setLoading(bool loading);
  void clear();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
  std::vector<ChatMessage> messages;
};
