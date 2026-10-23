// qBinEdit - small dialogs and text helpers
#pragma once

#include <QByteArray>
#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QLabel;

// Parse a number. Accepts 0x / h suffix for hex, 0b for binary, 0o for octal.
// Without prefix the number is hex when hexDefault is true, otherwise decimal.
bool parseNumber(const QString &text, bool hexDefault, quint64 *out);

// "DE AD be ef", "0xDE,0xAD", "DEADBEEF" -> bytes. Returns false on odd/invalid input.
bool parseHexBytes(const QString &text, QByteArray *out);

QString toHexText(const QByteArray &data, int bytesPerLine = 16);
QString toCArray(const QByteArray &data, const QString &name = QStringLiteral("data"));
QString toBinaryText(const QByteArray &data);

// Go to offset/address -----------------------------------------------------------
class GotoDialog : public QDialog
{
    Q_OBJECT
public:
    GotoDialog(quint64 baseAddress, qint64 size, qint64 current, QWidget *parent = nullptr);
    qint64 offset() const { return m_offset; }

private:
    void validate();

    quint64 m_base;
    qint64 m_size;
    qint64 m_current;
    qint64 m_offset = -1;
    QLineEdit *m_edit;
    QRadioButton *m_hex;
    QRadioButton *m_dec;
    QRadioButton *m_asOffset;
    QRadioButton *m_asAddress;
    QLabel *m_hint;
};

// Insert / fill bytes ------------------------------------------------------------
class BytesDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { Insert, Fill };
    BytesDialog(Mode mode, qint64 defaultCount, QWidget *parent = nullptr);
    QByteArray pattern() const { return m_pattern; }
    qint64 count() const;

private:
    QLineEdit *m_pattern_edit;
    QSpinBox *m_count;
    QByteArray m_pattern;
};
