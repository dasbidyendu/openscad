#pragma once

#include <QStyledItemDelegate>

class ChatDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  ChatDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
  static constexpr int padding = 10;
  static constexpr int cornerRadius = 12;
};
