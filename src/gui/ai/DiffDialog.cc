#include "gui/ai/DiffDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "gui/qtgettext.h"

DiffDialog::DiffDialog(const QString& oldCode, const QString& newCode, QWidget *parent)
  : QDialog(parent), oldCode(oldCode), newCode(newCode)
{
  setWindowTitle(_("Review AI Proposed Changes"));
  resize(800, 600);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  this->diffDisplay = new QTextEdit(this);
  this->diffDisplay->setReadOnly(true);
  this->diffDisplay->setFontFamily("Courier New");  // Use a fixed-width font for code
  this->diffDisplay->setLineWrapMode(QTextEdit::NoWrap);

  mainLayout->addWidget(new QLabel(_("Below are the proposed changes (surgical fix):"), this));
  mainLayout->addWidget(this->diffDisplay);

  QHBoxLayout *btnLayout = new QHBoxLayout();
  this->applyButton = new QPushButton(_("Apply Changes"), this);
  this->applyButton->setDefault(true);
  this->cancelButton = new QPushButton(_("Keep Original"), this);

  btnLayout->addStretch();
  btnLayout->addWidget(this->cancelButton);
  btnLayout->addWidget(this->applyButton);
  mainLayout->addLayout(btnLayout);

  connect(this->applyButton, &QPushButton::clicked, this, &DiffDialog::onApplyClicked);
  connect(this->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  computeDiff();
}

DiffDialog::~DiffDialog()
{
}

void DiffDialog::onApplyClicked()
{
  accept();
}

void DiffDialog::computeDiff()
{
  QStringList oldLines = oldCode.split('\n');
  QStringList newLines = newCode.split('\n');

  // Simple line-by-line diff for surgical fixes
  // Note: For a production-ready diff, we'd use LCS (Longest Common Subsequence).
  // Given the "surgical" constraint, we can just show the added/removed lines in a unified-like view.

  QString html = "<html><body style='white-space: pre; font-family: monospace;'>";

  // Minimalist unified diff generation
  int maxLines = std::max(oldLines.size(), newLines.size());
  for (int i = 0; i < maxLines; ++i) {
    QString oldLine = (i < oldLines.size()) ? oldLines[i] : "";
    QString newLine = (i < newLines.size()) ? newLines[i] : "";

    if (oldLine == newLine) {
      html += QString("<div style='color: #666;'>  %1</div>").arg(oldLine.toHtmlEscaped());
    } else {
      if (i < oldLines.size() && !oldLine.isEmpty()) {
        html += QString("<div style='background-color: #ffeef0; color: #b31d28;'>- %1</div>")
                  .arg(oldLine.toHtmlEscaped());
      }
      if (i < newLines.size() && !newLine.isEmpty()) {
        html += QString("<div style='background-color: #e6ffed; color: #22863a;'>+ %1</div>")
                  .arg(newLine.toHtmlEscaped());
      }
    }
  }

  html += "</body></html>";
  this->diffDisplay->setHtml(html);
}
