// qBinEdit - tab pages: single-file editor and two-file compare
#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QVector>
#include <QWidget>

class Document;
class HexView;
class QLabel;
class QTreeWidget;

class TabPage : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;
    virtual QList<HexView *> views() const = 0;
    virtual HexView *currentView() const = 0;
    virtual Document *document() const { return nullptr; }   // editable document, if any
    virtual QString title() const = 0;
    virtual QString toolTip() const { return title(); }

signals:
    void titleChanged();
    void activeViewChanged(HexView *view);
};

class EditorPage : public TabPage
{
    Q_OBJECT
public:
    EditorPage(Document *doc, QWidget *parent = nullptr);   // takes ownership of doc
    QList<HexView *> views() const override;
    HexView *currentView() const override { return m_view; }
    Document *document() const override { return m_doc; }
    QString title() const override;
    QString toolTip() const override;
    void refreshInfo();

private:
    Document *m_doc;
    HexView *m_view;
    QLabel *m_info;
};

class ComparePage : public TabPage
{
    Q_OBJECT
public:
    ComparePage(Document *left, Document *right, QWidget *parent = nullptr);   // takes ownership
    QList<HexView *> views() const override;
    HexView *currentView() const override;
    QString title() const override;
    QString toolTip() const override;

    void nextDifference();
    void previousDifference();
    qint64 differenceCount() const { return m_diffBytes; }

private:
    void computeDiffs();
    void jumpTo(int index);
    int indexAt(qint64 pos, bool forward) const;

    Document *m_left;
    Document *m_right;
    HexView *m_lview;
    HexView *m_rview;
    QTreeWidget *m_list;
    QLabel *m_summary;
    QVector<QPair<qint64, qint64>> m_diffs;   // (offset, length)
    qint64 m_diffBytes = 0;
    bool m_truncated = false;
    HexView *m_active;
    bool m_syncing = false;
};
