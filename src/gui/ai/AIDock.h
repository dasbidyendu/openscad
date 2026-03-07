#pragma once

#include "gui/Dock.h"

class QWidget;
class QVBoxLayout;
class QListView;
class QPlainTextEdit;
class ChatModel;
class ChatDelegate;
class QPushButton;
class QLabel;
#include <QJsonArray>

class AIDock : public Dock
{
  Q_OBJECT

public:
  AIDock(QWidget *parent = nullptr);
  virtual ~AIDock();

  void addMessage(const QString& text, bool isUser);
  void proposeCode(const QString& code);
  void setLoading(bool loading);
  QJsonArray getHistoryAsJson() const;

signals:
  void messageSubmitted(const QString& text);
  void applyCodeRequested(const QString& code);
  void reviewCodeRequested(const QString& code);
  void triggerPreviewRequested();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  QWidget *centralWidget;
  QVBoxLayout *layout;
  QListView *chatView;
  QPlainTextEdit *inputField;

  ChatModel *model;
  ChatDelegate *delegate;

  QWidget *applyPanel;
  QPushButton *reviewButton;
  QPushButton *applyButton;
  QLabel *applyLabel;
  QString currentProposedCode;
};
