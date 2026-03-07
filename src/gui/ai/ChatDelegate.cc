#include "gui/ai/ChatDelegate.h"
#include "gui/ai/ChatModel.h"

#include <QPainter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPalette>
#include <QApplication>

ChatDelegate::ChatDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void ChatDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const
{
  QString text = index.data(ChatModel::ChatRoles::TextRole).toString();
  bool isUser = index.data(ChatModel::ChatRoles::IsUserRole).toBool();
  bool isLoading = index.data(ChatModel::ChatRoles::IsLoadingRole).toBool();

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QPalette palette = QApplication::palette();
  bool isDark = palette.color(QPalette::Window).lightness() < 128;

  QColor userBubbleColor(0, 122, 255);
  QColor aiBubbleColor = isDark ? QColor(64, 65, 68) : QColor(230, 232, 235);

  QColor userTextColor = Qt::white;
  QColor aiTextColor = isDark ? Qt::white : Qt::black;
  QColor textColor = isUser ? userTextColor : aiTextColor;

  QTextDocument doc;
  doc.setDefaultStyleSheet(
    QString("body { color: %1; font-family: sans-serif; font-size: 13px; }").arg(textColor.name()));
  doc.setHtml(text);

  int maxWidth = option.rect.width() * 0.8;
  if (maxWidth <= 0) maxWidth = 200;
  doc.setTextWidth(maxWidth);

  QSize size = doc.size().toSize();
  int bubbleWidth = size.width() + 2 * padding;
  int bubbleHeight = size.height() + padding;

  QRect bubbleRect;
  if (isUser) {
    bubbleRect = QRect(option.rect.right() - bubbleWidth - padding, option.rect.top() + padding / 2,
                       bubbleWidth, bubbleHeight);
  } else {
    bubbleRect =
      QRect(option.rect.left() + padding, option.rect.top() + padding / 2, bubbleWidth, bubbleHeight);
  }

  painter->setBrush(isUser ? userBubbleColor : aiBubbleColor);
  painter->setPen(Qt::NoPen);

  if (isLoading) {
    // Render a "skeleton" bubble with multiple lines to simulate thinking
    int skeletonPadding = 8;
    int lineH = 12;
    int spacing = 6;
    int totalH = (3 * lineH) + (2 * spacing) + (2 * skeletonPadding);
    QRect skeletonBubble(option.rect.left() + padding, option.rect.top() + padding / 2, 180, totalH);

    painter->setBrush(aiBubbleColor);
    painter->drawRoundedRect(skeletonBubble, cornerRadius, cornerRadius);

    painter->setBrush(isDark ? QColor(80, 80, 85) : QColor(210, 212, 215));
    // Line 1: Long
    painter->drawRoundedRect(skeletonBubble.left() + skeletonPadding,
                             skeletonBubble.top() + skeletonPadding, 140, lineH, 4, 4);
    // Line 2: Medium
    painter->drawRoundedRect(skeletonBubble.left() + skeletonPadding,
                             skeletonBubble.top() + skeletonPadding + lineH + spacing, 100, lineH, 4, 4);
    // Line 3: Short
    painter->drawRoundedRect(skeletonBubble.left() + skeletonPadding,
                             skeletonBubble.top() + skeletonPadding + 2 * (lineH + spacing), 60, lineH,
                             4, 4);
    return;
  }

  painter->drawRoundedRect(bubbleRect, cornerRadius, cornerRadius);

  painter->translate(bubbleRect.left() + padding, bubbleRect.top() + padding / 2);
  doc.drawContents(painter);

  painter->restore();
}

QSize ChatDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  QString text = index.data(ChatModel::ChatRoles::TextRole).toString();
  int maxWidth = option.rect.width() * 0.75;
  if (maxWidth <= 0) maxWidth = 300;

  bool isLoading = index.data(ChatModel::ChatRoles::IsLoadingRole).toBool();
  if (isLoading) return QSize(option.rect.width(), 80);

  QTextDocument doc;
  doc.setHtml(text);
  doc.setTextWidth(maxWidth);

  return QSize(option.rect.width(), (int)doc.size().height() + 2 * padding);
}
