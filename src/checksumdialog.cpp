// qBinEdit - checksum / CRC dialog
#include "checksumdialog.h"
#include "checksums.h"
#include "dialogs.h"
#include "document.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

ChecksumDialog::ChecksumDialog(Document *doc, qint64 selStart, qint64 selLength, QWidget *parent)
    : QDialog(parent), m_doc(doc), m_selStart(selStart), m_selLength(selLength)
{
    setWindowTitle(tr("Checksum / CRC - %1").arg(doc->displayName()));
    resize(720, 640);

    auto *range = new QGroupBox(tr("Range"), this);
    m_whole = new QRadioButton(tr("Whole file (%1 bytes)").arg(QLocale().toString(doc->size())), range);
    m_sel = new QRadioButton(tr("Selection"), range);
    m_custom = new QRadioButton(tr("Custom offsets (hex, inclusive):"), range);
    m_from = new QLineEdit(range);
    m_to = new QLineEdit(range);
    m_from->setPlaceholderText(tr("from, e.g. 0"));
    m_to->setPlaceholderText(tr("to, e.g. 7FFF"));
    m_rangeInfo = new QLabel(range);

    if (selLength > 0) {
        m_sel->setText(tr("Selection: 0x%1 - 0x%2 (%3 bytes)")
                           .arg(QString::number(selStart, 16).toUpper(),
                                QString::number(selStart + selLength - 1, 16).toUpper(),
                                QLocale().toString(selLength)));
        m_sel->setChecked(true);
        m_from->setText(QString::number(selStart, 16).toUpper());
        m_to->setText(QString::number(selStart + selLength - 1, 16).toUpper());
    } else {
        m_sel->setEnabled(false);
        m_whole->setChecked(true);
        m_from->setText(QStringLiteral("0"));
        m_to->setText(QString::number(qMax<qint64>(0, doc->size() - 1), 16).toUpper());
    }

    auto *rg = new QGridLayout(range);
    rg->addWidget(m_whole, 0, 0, 1, 3);
    rg->addWidget(m_sel, 1, 0, 1, 3);
    rg->addWidget(m_custom, 2, 0);
    rg->addWidget(m_from, 2, 1);
    rg->addWidget(m_to, 2, 2);
    rg->addWidget(m_rangeInfo, 3, 0, 1, 3);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({ tr("Algorithm"), tr("Result") });
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 6);

    auto *note = new QLabel(tr("Bytes in address gaps of HEX/S-Record files are included with the fill value 0xFF."), this);
    note->setWordWrap(true);
    note->setVisible(doc->hasGaps());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *calc = buttons->addButton(tr("Calculate"), QDialogButtonBox::ActionRole);
    QPushButton *copy = buttons->addButton(tr("Copy all"), QDialogButtonBox::ActionRole);
    calc->setDefault(true);
    connect(calc, &QPushButton::clicked, this, &ChecksumDialog::calculate);
    connect(copy, &QPushButton::clicked, this, &ChecksumDialog::copyAll);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_from, &QLineEdit::textEdited, this, [this]() { m_custom->setChecked(true); });
    connect(m_to, &QLineEdit::textEdited, this, [this]() { m_custom->setChecked(true); });
    for (QRadioButton *b : { m_whole, m_sel, m_custom })
        connect(b, &QRadioButton::toggled, this, [this](bool on) { if (on) calculate(); });

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(range);
    lay->addWidget(m_table, 1);
    lay->addWidget(note);
    lay->addWidget(buttons);

    calculate();
}

void ChecksumDialog::calculate()
{
    if (!m_doc)
        return;
    const qint64 size = m_doc->size();
    qint64 from = 0, len = size;
    if (m_sel->isChecked()) {
        from = m_selStart;
        len = m_selLength;
    } else if (m_custom->isChecked()) {
        quint64 a = 0, b = 0;
        if (!parseNumber(m_from->text(), true, &a) || !parseNumber(m_to->text(), true, &b) || b < a
            || qint64(b) >= size) {
            m_rangeInfo->setText(tr("<span style='color:#d32f2f'>Invalid range. Offsets must be within 0 - %1 (hex).</span>")
                                     .arg(QString::number(qMax<qint64>(0, size - 1), 16).toUpper()));
            return;
        }
        from = qint64(a);
        len = qint64(b - a + 1);
    }
    m_rangeInfo->setText(tr("Calculating over offset 0x%1, length %2 bytes (address 0x%3).")
                             .arg(QString::number(from, 16).toUpper(), QLocale().toString(len),
                                  QString::number(m_doc->baseAddress() + quint64(from), 16).toUpper()));

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto *p = reinterpret_cast<const uchar *>(m_doc->data().constData()) + from;
    const QVector<Checksums::Result> res = Checksums::computeAll(p, len);
    QApplication::restoreOverrideCursor();

    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    m_table->setRowCount(res.size());
    for (int i = 0; i < res.size(); ++i) {
        auto *n = new QTableWidgetItem(res[i].name);
        n->setToolTip(res[i].note);
        m_table->setItem(i, 0, n);
        auto *v = new QTableWidgetItem(res[i].value);
        v->setFont(mono);
        v->setToolTip(res[i].note);
        m_table->setItem(i, 1, v);
    }
}

void ChecksumDialog::copyAll()
{
    QString out = m_rangeInfo->text() + QLatin1Char('\n');
    for (int r = 0; r < m_table->rowCount(); ++r)
        out += m_table->item(r, 0)->text() + QLatin1Char('\t') + m_table->item(r, 1)->text() + QLatin1Char('\n');
    QApplication::clipboard()->setText(out);
}
