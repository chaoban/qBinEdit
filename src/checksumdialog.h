// qBinEdit - checksum / CRC dialog
#pragma once

#include <QDialog>
#include <QPointer>

class Document;
class QLineEdit;
class QRadioButton;
class QTableWidget;
class QLabel;

class ChecksumDialog : public QDialog
{
    Q_OBJECT
public:
    ChecksumDialog(Document *doc, qint64 selStart, qint64 selLength, QWidget *parent = nullptr);

private:
    void calculate();
    void copyAll();

    QPointer<Document> m_doc;
    qint64 m_selStart;
    qint64 m_selLength;
    QRadioButton *m_whole;
    QRadioButton *m_sel;
    QRadioButton *m_custom;
    QLineEdit *m_from;
    QLineEdit *m_to;
    QLabel *m_rangeInfo;
    QTableWidget *m_table;
};
