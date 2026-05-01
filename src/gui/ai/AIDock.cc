#include "gui/ai/AIDock.h"
#include "gui/ai/ChatModel.h"
#include "gui/ai/ChatDelegate.h"
#include "core/Settings.h"
#include <QShowEvent>

#include <QVBoxLayout>
#include <QListView>
#include <QPlainTextEdit>
#include <QWidget>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonObject>

#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QRegularExpression>
#include "gui/qtgettext.h"

AIDock::AIDock(QWidget *parent) : Dock(parent)
{
  this->centralWidget = new QWidget(this);
  this->layout = new QVBoxLayout(this->centralWidget);

  QHBoxLayout *topLay = new QHBoxLayout();
  this->newChatButton = new QPushButton(_("New Chat"), this->centralWidget);
  connect(this->newChatButton, &QPushButton::clicked, this, &AIDock::clearChat);
  topLay->addStretch();
  topLay->addWidget(this->newChatButton);

  this->chatView = new QListView(this->centralWidget);
  this->model = new ChatModel(this);
  this->delegate = new ChatDelegate(this);

  this->chatView->setModel(this->model);
  this->chatView->setItemDelegate(this->delegate);
  this->chatView->setSelectionMode(QAbstractItemView::NoSelection);
  this->chatView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  this->chatView->setSpacing(5);

  this->chatView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this->chatView, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
    QModelIndex index = this->chatView->indexAt(pos);
    if (index.isValid()) {
      QMenu menu;
      QAction *copyAction = menu.addAction(_("Copy Message"));
      QAction *copyCodeAction = menu.addAction(_("Copy Code Blocks Only"));

      QAction *selectedItem = menu.exec(this->chatView->viewport()->mapToGlobal(pos));
      if (selectedItem == copyAction) {
        QString text = index.data(ChatModel::ChatRoles::TextRole).toString();
        QApplication::clipboard()->setText(text);
      } else if (selectedItem == copyCodeAction) {
        QString text = index.data(ChatModel::ChatRoles::TextRole).toString();
        QRegularExpression regex("```(?:\\w*\\n)?(.*?)```",
                                 QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator i = regex.globalMatch(text);
        QStringList codes;
        while (i.hasNext()) {
          QRegularExpressionMatch match = i.next();
          codes << match.captured(1).trimmed();
        }
        if (!codes.isEmpty()) {
          QApplication::clipboard()->setText(codes.join("\n\n"));
        } else {
          QApplication::clipboard()->setText(text);  // Fallback to all if no code blocks
        }
      }
    }
  });

  this->inputField = new QPlainTextEdit(this->centralWidget);
  this->inputField->setMaximumHeight(100);
  this->inputField->setPlaceholderText(_("Type your question here..."));
  this->inputField->installEventFilter(this);

  this->applyPanel = new QWidget(this->centralWidget);
  this->applyPanel->setObjectName("applyPanel");
  this->applyPanel->setStyleSheet(
    "QWidget#applyPanel { background-color: #3e3e42; border-top: 1px solid #555; }"
    "QLabel { color: #ccc; font-size: 11px; }"
    "QPushButton { background-color: #007aff; color: white; border-radius: 4px; padding: 4px 12px; "
    "font-weight: bold; border: none; }"
    "QPushButton:hover { background-color: #0062cc; }");
  QHBoxLayout *applyLay = new QHBoxLayout(this->applyPanel);
  applyLay->setContentsMargins(10, 5, 10, 5);
  this->applyLabel = new QLabel(_("AI proposed a code change."), this->applyPanel);
  this->reviewButton = new QPushButton(_("Review"), this->applyPanel);
  this->applyButton = new QPushButton(_("Apply"), this->applyPanel);
  applyLay->addWidget(this->applyLabel);
  applyLay->addStretch();
  applyLay->addWidget(this->reviewButton);
  applyLay->addWidget(this->applyButton);
  this->applyPanel->hide();

  this->layout->addLayout(topLay);
  this->layout->addWidget(this->chatView);
  this->layout->addWidget(this->applyPanel);
  this->layout->addWidget(this->inputField);

  connect(this->reviewButton, &QPushButton::clicked, this,
          [this]() { emit reviewCodeRequested(this->currentProposedCode); });

  connect(this->applyButton, &QPushButton::clicked, this, [this]() {
    emit applyCodeRequested(this->currentProposedCode);
    this->applyPanel->hide();
  });

  setWidget(this->centralWidget);

  // Dummy messages for verification
  this->model->addMessage(
    _("Hello! I'm your AI Assistant. How can I help you with your OpenSCAD model today?"), false);
  this->model->addMessage(_("Can you help me create a parametric box?"), true);
  this->model->addMessage(
    _("Certainly! I can use tools to help you create that. Just let me know the dimensions."), false);
}

AIDock::~AIDock()
{
  delete this->centralWidget;
}

void AIDock::clearChat()
{
  this->model->clear();
  this->model->addMessage(
    _("Hello! I'm your AI Assistant. How can I help you with your OpenSCAD model today?"), false);
}

void AIDock::updateTitleWithModel()
{
  QString modelName = QString::fromStdString(Settings::Settings::aiModel.value());
  if (modelName.isEmpty()) modelName = "AI Chat";
  else modelName = QString("AI Chat (%1)").arg(modelName);
  this->setWindowTitle(modelName);
  this->setName(modelName);
}

void AIDock::showEvent(QShowEvent *event)
{
  updateTitleWithModel();
  Dock::showEvent(event);
}

void AIDock::addMessage(const QString& text, bool isUser)
{
  this->model->addMessage(text, isUser);
  this->chatView->scrollToBottom();
}

void AIDock::proposeCode(const QString& code)
{
  this->currentProposedCode = code;
  this->applyPanel->show();
  this->addMessage(_("AI proposed code changes. [Apply]"), false);
}

void AIDock::setLoading(bool loading)
{
  this->model->setLoading(loading);
  this->chatView->scrollToBottom();
}

bool AIDock::eventFilter(QObject *obj, QEvent *event)
{
  if (obj == this->inputField && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
      if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
        QString text = this->inputField->toPlainText().trimmed();
        if (!text.isEmpty()) {
          this->model->addMessage(text, true);
          emit messageSubmitted(text);
          this->inputField->clear();
        }
        return true;
      }
    }
  }
  return Dock::eventFilter(obj, event);
}

QJsonArray AIDock::getHistoryAsJson() const
{
  QJsonArray history;
  for (int i = 0; i < this->model->rowCount(); ++i) {
    bool isUser = this->model->data(this->model->index(i), ChatModel::IsUserRole).toBool();
    QString text = this->model->data(this->model->index(i), ChatModel::TextRole).toString();

    // Skip UI logs/action messages that aren't real AI content to avoid protocol errors
    if (!isUser && text.contains("[Apply]")) continue;

    QJsonObject msg;
    msg["role"] = isUser ? "user" : "assistant";
    msg["content"] = text;
    history.append(msg);
  }
  return history;
}
