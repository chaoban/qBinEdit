// qBinEdit - small dialogs and text helpers
#include "dialogs.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QButtonGroup>
#include <QVBoxLayout>

bool parseNumber(const QString &text, bool hexDefault, quint64 *out)
{
    QString t = text.trimmed().remove(QLatin1Char('_')).remove(QLatin1Char(' ')).toLower();
    if (t.isEmpty())
        return false;
    int base = hexDefault ? 16 : 10;
    if (t.startsWith(QLatin1String("0x"))) {
        base = 16;
        t = t.mid(2);
    } else if (!hexDefault && t.startsWith(QLatin1String("0b"))) {   // "0b12" is valid hex
        base = 2;
        t = t.mid(2);
    } else if (!hexDefault && t.startsWith(QLatin1String("0o"))) {
        base = 8;
        t = t.mid(2);
    } else if (t.endsWith(QLatin1Char('h'))) {
        base = 16;
        t.chop(1);
    }
    bool ok = false;
    const quint64 v = t.toULongLong(&ok, base);
    if (ok)
        *out = v;
    return ok;
}

bool parseHexBytes(const QString &text, QByteArray *out)
{
    QString t = text;
    t.replace(QRegularExpression(QStringLiteral("0[xX]")), QStringLiteral(" "));
    t.remove(QRegularExpression(QStringLiteral("[\\s,;:\\-{}]")));
    if (t.isEmpty() || (t.size() & 1))
        return false;
    static const QRegularExpression valid(QStringLiteral("^[0-9a-fA-F]+$"));
    if (!valid.match(t).hasMatch())
        return false;
    *out = QByteArray::fromHex(t.toLatin1());
    return true;
}

// The text converters below write straight into a pre-sized QString: a 32 MiB selection
// would otherwise create tens of millions of temporary strings.
namespace {
const char16_t kHexDigits[] = u"0123456789ABCDEF";
}

QString toHexText(const QByteArray &data, int bytesPerLine)
{
    const qsizetype n = data.size();
    if (n == 0)
        return QString();
    QString s(n * 3 - 1, Qt::Uninitialized);
    QChar *o = s.data();
    for (qsizetype i = 0; i < n; ++i) {
        if (i)
            *o++ = (bytesPerLine > 0 && i % bytesPerLine == 0) ? QLatin1Char('\n') : QLatin1Char(' ');
        const uchar b = uchar(data.at(i));
        *o++ = QChar(kHexDigits[b >> 4]);
        *o++ = QChar(kHexDigits[b & 0xF]);
    }
    return s;
}

QString toCArray(const QByteArray &data, const QString &name)
{
    const QString head = QStringLiteral("const unsigned char %1[%2] = {\n").arg(name).arg(data.size());
    const QString tail = QStringLiteral("};\n");
    const qsizetype n = data.size();
    // per byte "0xAB," + ' ' or '\n' (6 chars, the last one has no comma) + 4 spaces per line
    QString s(head.size() + n * 6 + ((n + 15) / 16) * 4 + tail.size(), Qt::Uninitialized);
    QChar *o = s.data();
    for (QChar c : head)
        *o++ = c;
    for (qsizetype i = 0; i < n; ++i) {
        if (i % 16 == 0)
            for (int k = 0; k < 4; ++k)
                *o++ = QLatin1Char(' ');
        const uchar b = uchar(data.at(i));
        *o++ = QLatin1Char('0');
        *o++ = QLatin1Char('x');
        *o++ = QChar(kHexDigits[b >> 4]);
        *o++ = QChar(kHexDigits[b & 0xF]);
        if (i + 1 < n)
            *o++ = QLatin1Char(',');
        *o++ = (i % 16 == 15 || i + 1 == n) ? QLatin1Char('\n') : QLatin1Char(' ');
    }
    for (QChar c : tail)
        *o++ = c;
    s.truncate(o - s.constData());
    return s;
}

QString toBinaryText(const QByteArray &data)
{
    const qsizetype n = data.size();
    if (n == 0)
        return QString();
    QString s(n * 9 - 1, Qt::Uninitialized);
    QChar *o = s.data();
    for (qsizetype i = 0; i < n; ++i) {
        if (i)
            *o++ = QLatin1Char(' ');
        const uchar b = uchar(data.at(i));
        for (int bit = 7; bit >= 0; --bit)
            *o++ = QLatin1Char((b >> bit) & 1 ? '1' : '0');
    }
    return s;
}

// ============================================================================

GotoDialog::GotoDialog(quint64 baseAddress, qint64 size, qint64 current, QWidget *parent)
    : QDialog(parent), m_base(baseAddress), m_size(size), m_current(current)
{
    setWindowTitle(tr("Go To"));
    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("e.g. 1F00, 0x08001000, +100, -20"));
    m_hex = new QRadioButton(tr("Hexadecimal"), this);
    m_dec = new QRadioButton(tr("Decimal"), this);
    m_hex->setChecked(true);
    m_asOffset = new QRadioButton(tr("File offset"), this);
    m_asAddress = new QRadioButton(tr("Address (base 0x%1)").arg(QString::number(baseAddress, 16).toUpper()), this);
    (baseAddress ? m_asAddress : m_asOffset)->setChecked(true);
    m_hint = new QLabel(this);

    auto *g1 = new QButtonGroup(this);
    g1->addButton(m_hex);
    g1->addButton(m_dec);
    auto *g2 = new QButtonGroup(this);
    g2->addButton(m_asOffset);
    g2->addButton(m_asAddress);

    auto *row1 = new QHBoxLayout;
    row1->addWidget(m_hex);
    row1->addWidget(m_dec);
    row1->addStretch();
    auto *row2 = new QHBoxLayout;
    row2->addWidget(m_asOffset);
    row2->addWidget(m_asAddress);
    row2->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        validate();
        if (m_offset >= 0)
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_edit, &QLineEdit::textChanged, this, &GotoDialog::validate);
    connect(m_hex, &QRadioButton::toggled, this, &GotoDialog::validate);
    connect(m_asOffset, &QRadioButton::toggled, this, &GotoDialog::validate);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(tr("Go to (prefix + / - for a relative jump):"), this));
    lay->addWidget(m_edit);
    lay->addLayout(row1);
    lay->addLayout(row2);
    lay->addWidget(m_hint);
    lay->addWidget(buttons);
    validate();
}

void GotoDialog::validate()
{
    m_offset = -1;
    QString t = m_edit->text().trimmed();
    if (t.isEmpty()) {
        m_hint->setText(tr("Valid offsets: 0 - 0x%1").arg(QString::number(qMax<qint64>(0, m_size - 1), 16).toUpper()));
        return;
    }
    int sign = 0;
    if (t.startsWith(QLatin1Char('+'))) { sign = 1; t = t.mid(1); }
    else if (t.startsWith(QLatin1Char('-'))) { sign = -1; t = t.mid(1); }
    quint64 v = 0;
    if (!parseNumber(t, m_hex->isChecked(), &v)) {
        m_hint->setText(tr("<span style='color:#d32f2f'>Not a valid number.</span>"));
        return;
    }
    qint64 off;
    if (sign)
        off = m_current + sign * qint64(v);
    else if (m_asAddress->isChecked())
        off = qint64(v - m_base);
    else
        off = qint64(v);
    if ((m_asAddress->isChecked() && !sign && v < m_base) || off < 0 || off >= qMax<qint64>(1, m_size)) {
        m_hint->setText(tr("<span style='color:#d32f2f'>Out of range.</span>"));
        return;
    }
    m_offset = off;
    m_hint->setText(tr("Offset 0x%1 = address 0x%2")
                        .arg(QString::number(off, 16).toUpper(), QString::number(m_base + quint64(off), 16).toUpper()));
}

// ============================================================================

BytesDialog::BytesDialog(Mode mode, qint64 defaultCount, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(mode == Insert ? tr("Insert Bytes") : tr("Fill Selection"));
    m_pattern_edit = new QLineEdit(QStringLiteral("FF"), this);
    m_pattern_edit->setToolTip(tr("Hex byte pattern, repeated to fill the range (e.g. FF or DE AD BE EF)."));
    m_count = new QSpinBox(this);
    m_count->setRange(1, 64 * 1024 * 1024);
    m_count->setValue(int(qMin<qint64>(qMax<qint64>(1, defaultCount), 64 * 1024 * 1024)));
    m_count->setEnabled(mode == Insert);

    auto *form = new QFormLayout;
    form->addRow(tr("Pattern (hex):"), m_pattern_edit);
    form->addRow(tr("Byte count:"), m_count);

    auto *err = new QLabel(this);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, err]() {
        if (!parseHexBytes(m_pattern_edit->text(), &m_pattern)) {
            err->setText(tr("<span style='color:#d32f2f'>Enter whole hex bytes, e.g. FF or 00 11.</span>"));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(err);
    lay->addWidget(buttons);
}

qint64 BytesDialog::count() const
{
    return m_count->value();
}
