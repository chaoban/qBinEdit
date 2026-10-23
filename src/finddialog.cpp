// qBinEdit - find / replace dialog (non-modal)
#include "finddialog.h"
#include "dialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FindDialog::FindDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Find / Replace"));
    setModal(false);

    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Hex bytes"));
    m_mode->addItem(tr("ASCII text"));
    m_mode->addItem(tr("UTF-16LE text"));
    m_find = new QLineEdit(this);
    m_find->setPlaceholderText(tr("e.g. DE AD BE EF"));
    m_replace = new QLineEdit(this);
    m_replaceLabel = new QLabel(tr("Replace with:"), this);
    m_case = new QCheckBox(tr("Case sensitive"), this);
    m_case->setChecked(true);
    m_case->setEnabled(false);
    m_result = new QLabel(this);
    m_result->setWordWrap(true);

    auto *btnNext = new QPushButton(tr("Find &Next"), this);
    auto *btnPrev = new QPushButton(tr("Find &Previous"), this);
    auto *btnCount = new QPushButton(tr("&Count"), this);
    m_btnReplace = new QPushButton(tr("&Replace"), this);
    m_btnReplaceAll = new QPushButton(tr("Replace &All"), this);
    auto *btnClose = new QPushButton(tr("Close"), this);
    btnNext->setDefault(true);

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Search as:"), this), 0, 0);
    grid->addWidget(m_mode, 0, 1);
    grid->addWidget(new QLabel(tr("Find what:"), this), 1, 0);
    grid->addWidget(m_find, 1, 1);
    grid->addWidget(m_replaceLabel, 2, 0);
    grid->addWidget(m_replace, 2, 1);
    grid->addWidget(m_case, 3, 1);
    grid->setColumnStretch(1, 1);

    auto *buttons = new QVBoxLayout;
    for (QPushButton *b : { btnNext, btnPrev, btnCount, m_btnReplace, m_btnReplaceAll })
        buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(btnClose);

    auto *top = new QHBoxLayout;
    top->addLayout(grid, 1);
    top->addLayout(buttons);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(top);
    lay->addWidget(m_result);

    connect(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        m_case->setEnabled(i != HexBytes);
        m_find->setPlaceholderText(i == HexBytes ? tr("e.g. DE AD BE EF") : tr("text"));
    });
    connect(btnNext, &QPushButton::clicked, this, &FindDialog::findNext);
    connect(btnPrev, &QPushButton::clicked, this, &FindDialog::findPrevious);
    connect(btnCount, &QPushButton::clicked, this, &FindDialog::countAll);
    connect(m_btnReplace, &QPushButton::clicked, this, &FindDialog::replaceOne);
    connect(m_btnReplaceAll, &QPushButton::clicked, this, &FindDialog::replaceAll);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::hide);
}

void FindDialog::setReplaceVisible(bool on)
{
    m_replace->setVisible(on);
    m_replaceLabel->setVisible(on);
    m_btnReplace->setVisible(on);
    m_btnReplaceAll->setVisible(on);
    setWindowTitle(on ? tr("Find / Replace") : tr("Find"));
    adjustSize();
}

void FindDialog::setFindText(const QString &text, Mode mode)
{
    m_mode->setCurrentIndex(int(mode));
    m_find->setText(text);
    m_find->selectAll();
    m_find->setFocus();
}

bool FindDialog::caseSensitive() const
{
    return mode() == HexBytes || m_case->isChecked();
}

FindDialog::Mode FindDialog::mode() const
{
    return Mode(m_mode->currentIndex());
}

void FindDialog::showResult(const QString &msg, bool error)
{
    m_result->setText(error ? QStringLiteral("<span style='color:#d32f2f'>%1</span>").arg(msg.toHtmlEscaped()) : msg);
}

bool FindDialog::encode(const QString &text, QByteArray *out, QString *error) const
{
    switch (mode()) {
    case HexBytes:
        if (!parseHexBytes(text, out)) {
            *error = tr("Enter whole hex bytes, e.g. DE AD BE EF.");
            return false;
        }
        return true;
    case AsciiText:
        *out = text.toLatin1();
        break;
    case Utf16Text: {
        out->clear();
        for (QChar c : text) {
            out->append(char(c.unicode() & 0xFF));
            out->append(char(c.unicode() >> 8));
        }
        break;
    }
    }
    return true;
}

bool FindDialog::findPattern(QByteArray *out, QString *error) const
{
    if (m_find->text().isEmpty()) {
        *error = tr("Enter something to find.");
        return false;
    }
    return encode(m_find->text(), out, error);
}

bool FindDialog::replacePattern(QByteArray *out, QString *error) const
{
    if (m_replace->text().isEmpty()) {
        out->clear();
        return true;      // replace with nothing = delete
    }
    return encode(m_replace->text(), out, error);
}
