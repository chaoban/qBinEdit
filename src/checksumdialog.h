// qBinEdit - checksum / CRC dialog
#pragma once

#include "checksums.h"

#include <QDialog>
#include <QPointer>
#include <memory>

class Document;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QTableWidget;
class QTimer;
class QLabel;

class ChecksumDialog : public QDialog
{
    Q_OBJECT
public:
    ChecksumDialog(Document *doc, qint64 selStart, qint64 selLength, QWidget *parent = nullptr);
    ~ChecksumDialog() override;

private:
    struct Job;     // one background calculation

    void calculate();
    void showResults(const QVector<Checksums::Result> &res);
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
    QProgressBar *m_progress;
    QTimer *m_progressTimer;
    std::shared_ptr<Job> m_job;     // the calculation whose result will be shown
};
