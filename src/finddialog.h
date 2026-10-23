// qBinEdit - find / replace dialog (non-modal)
#pragma once

#include <QByteArray>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class FindDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FindDialog(QWidget *parent = nullptr);

    enum Mode { HexBytes = 0, AsciiText = 1, Utf16Text = 2 };

    void setReplaceVisible(bool on);
    void setFindText(const QString &text, Mode mode);
    bool findPattern(QByteArray *out, QString *error) const;
    bool replacePattern(QByteArray *out, QString *error) const;
    bool caseSensitive() const;
    Mode mode() const;
    void showResult(const QString &msg, bool error = false);

signals:
    void findNext();
    void findPrevious();
    void countAll();
    void replaceOne();
    void replaceAll();

private:
    bool encode(const QString &text, QByteArray *out, QString *error) const;

    QComboBox *m_mode;
    QLineEdit *m_find;
    QLineEdit *m_replace;
    QLabel *m_replaceLabel;
    QCheckBox *m_case;
    QPushButton *m_btnReplace;
    QPushButton *m_btnReplaceAll;
    QLabel *m_result;
};
