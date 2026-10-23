// qBinEdit - data inspector: interpret bytes at the cursor
#include "datainspector.h"
#include "document.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cstring>

namespace {

enum Row {
    RowBin8, RowBin16, RowOct8, RowHex16, RowHex32,
    RowU8, RowI8, RowU16, RowI16, RowU32, RowI32, RowU64, RowI64,
    RowF32, RowF64, RowChar, RowCount
};

} // namespace

DataInspector::DataInspector(QWidget *parent)
    : QWidget(parent)
{
    m_pos = new QLabel(this);
    m_big = new QCheckBox(tr("Big endian"), this);
    m_table = new QTableWidget(RowCount, 2, this);
    m_table->setHorizontalHeaderLabels({ tr("Type"), tr("Value") });
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 6);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    const QStringList names = {
        tr("Binary (8-bit)"), tr("Binary (16-bit)"), tr("Octal (8-bit)"), tr("Hex (16-bit)"), tr("Hex (32-bit)"),
        QStringLiteral("uint8"), QStringLiteral("int8"), QStringLiteral("uint16"), QStringLiteral("int16"),
        QStringLiteral("uint32"), QStringLiteral("int32"), QStringLiteral("uint64"), QStringLiteral("int64"),
        QStringLiteral("float32"), QStringLiteral("float64"), tr("Character")
    };
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    for (int r = 0; r < RowCount; ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(names.value(r)));
        auto *v = new QTableWidgetItem;
        v->setFont(mono);
        m_table->setItem(r, 1, v);
    }

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addWidget(m_pos);
    lay->addWidget(m_big);
    lay->addWidget(m_table, 1);

    connect(m_big, &QCheckBox::toggled, this, &DataInspector::refresh);
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &p) {
        QTableWidgetItem *it = m_table->itemAt(p);
        if (!it)
            return;
        const QString val = m_table->item(it->row(), 1)->text();
        QMenu menu(this);
        menu.addAction(tr("Copy value"), this, [val]() { QApplication::clipboard()->setText(val); });
        menu.exec(m_table->viewport()->mapToGlobal(p));
    });
    refresh();
}

void DataInspector::setSource(Document *doc, qint64 offset)
{
    m_doc = doc;
    m_offset = offset;
    refresh();
}

void DataInspector::refresh()
{
    const QString na = QStringLiteral("—");
    if (!m_doc) {
        m_pos->setText(tr("No file"));
        for (int r = 0; r < RowCount; ++r)
            m_table->item(r, 1)->setText(na);
        return;
    }
    const qint64 size = m_doc->size();
    const qint64 off = m_offset;
    m_pos->setText(tr("Offset 0x%1  ·  Address 0x%2")
                       .arg(QString::number(off, 16).toUpper())
                       .arg(QString::number(m_doc->baseAddress() + quint64(off), 16).toUpper()));
    const bool big = m_big->isChecked();
    const int avail = int(bound64(0, size - off, 8));
    uchar b[8] = {};
    for (int i = 0; i < avail; ++i)
        b[i] = m_doc->byteAt(off + i);

    auto get = [&](int n, quint64 *out) {
        if (avail < n)
            return false;
        quint64 v = 0;
        for (int i = 0; i < n; ++i)
            v |= quint64(big ? b[n - 1 - i] : b[i]) << (8 * i);
        *out = v;
        return true;
    };
    auto set = [&](int row, const QString &s) { m_table->item(row, 1)->setText(s); };
    auto bin = [](quint64 v, int bits) {
        QString s = QString::number(v, 2).rightJustified(bits, QLatin1Char('0'));
        for (int i = s.size() - 4; i > 0; i -= 4)
            s.insert(i, QLatin1Char(' '));
        return s;
    };

    quint64 v = 0;
    if (get(1, &v)) {
        set(RowBin8, bin(v, 8));
        set(RowOct8, QString::number(v, 8).rightJustified(3, QLatin1Char('0')));
        set(RowU8, QString::number(v));
        set(RowI8, QString::number(qint8(v)));
        const QChar c = (v >= 0x20 && v < 0x7F) ? QChar(char16_t(v)) : QChar();
        set(RowChar, c.isNull() ? QStringLiteral("0x") + QString::number(v, 16).toUpper().rightJustified(2, QLatin1Char('0')) : QStringLiteral("'%1'").arg(c));
    } else {
        for (int r : { RowBin8, RowOct8, RowU8, RowI8, RowChar })
            set(r, na);
    }
    if (get(2, &v)) {
        set(RowBin16, bin(v, 16));
        set(RowHex16, QString::number(v, 16).toUpper().rightJustified(4, QLatin1Char('0')));
        set(RowU16, QString::number(v));
        set(RowI16, QString::number(qint16(v)));
    } else {
        for (int r : { RowBin16, RowHex16, RowU16, RowI16 })
            set(r, na);
    }
    if (get(4, &v)) {
        set(RowHex32, QString::number(v, 16).toUpper().rightJustified(8, QLatin1Char('0')));
        set(RowU32, QString::number(v));
        set(RowI32, QString::number(qint32(v)));
        quint32 u32 = quint32(v);
        float f;
        std::memcpy(&f, &u32, 4);
        set(RowF32, QString::number(double(f), 'g', 9));
    } else {
        for (int r : { RowHex32, RowU32, RowI32, RowF32 })
            set(r, na);
    }
    if (get(8, &v)) {
        set(RowU64, QString::number(v));
        set(RowI64, QString::number(qint64(v)));
        double d;
        std::memcpy(&d, &v, 8);
        set(RowF64, QString::number(d, 'g', 17));
    } else {
        for (int r : { RowU64, RowI64, RowF64 })
            set(r, na);
    }
}
