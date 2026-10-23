// qBinEdit - data inspector: interpret bytes at the cursor
#pragma once

#include <QPointer>
#include <QWidget>

class Document;
class QCheckBox;
class QLabel;
class QTableWidget;

class DataInspector : public QWidget
{
    Q_OBJECT
public:
    explicit DataInspector(QWidget *parent = nullptr);
    void setSource(Document *doc, qint64 offset);
    void refresh();

private:
    QPointer<Document> m_doc;
    qint64 m_offset = 0;
    QLabel *m_pos;
    QCheckBox *m_big;
    QTableWidget *m_table;
};
