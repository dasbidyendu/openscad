#pragma once

#include <QDialog>
#include <QString>

class QTextEdit;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;

class DiffDialog : public QDialog
{
  Q_OBJECT

public:
  DiffDialog(const QString& oldCode, const QString& newCode, QWidget *parent = nullptr);
  virtual ~DiffDialog();

  QString getNewCode() const { return this->newCode; }

private slots:
  void onApplyClicked();

private:
  void computeDiff();

  QString oldCode;
  QString newCode;
  QTextEdit *diffDisplay;
  QPushButton *applyButton;
  QPushButton *cancelButton;
};
