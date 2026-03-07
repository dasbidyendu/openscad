#include "gui/ai/ChatModel.h"

ChatModel::ChatModel(QObject *parent) : QAbstractListModel(parent)
{
}

void ChatModel::addMessage(const QString& text, bool isUser)
{
  beginInsertRows(QModelIndex(), (int)messages.size(), (int)messages.size());
  messages.push_back({text, isUser, false});
  endInsertRows();
}

void ChatModel::setLoading(bool loading)
{
  bool currentlyLoading = !messages.empty() && messages.back().isLoading;
  if (loading == currentlyLoading) return;

  if (loading) {
    beginInsertRows(QModelIndex(), (int)messages.size(), (int)messages.size());
    messages.push_back({"", false, true});
    endInsertRows();
  } else {
    if (currentlyLoading) {
      beginRemoveRows(QModelIndex(), (int)messages.size() - 1, (int)messages.size() - 1);
      messages.pop_back();
      endRemoveRows();
    }
  }
}

void ChatModel::clear()
{
  beginResetModel();
  messages.clear();
  endResetModel();
}

int ChatModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) return 0;
  return (int)messages.size();
}

QVariant ChatModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() >= (int)messages.size()) return QVariant();

  const auto& msg = messages[index.row()];
  if (role == ChatRoles::TextRole || role == Qt::DisplayRole) return msg.text;
  if (role == ChatRoles::IsUserRole) return msg.isUser;
  if (role == ChatRoles::IsLoadingRole) return msg.isLoading;

  return QVariant();
}
