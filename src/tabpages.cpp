// qBinEdit - tab pages: single-file editor and two-file compare
#include "tabpages.h"
#include "document.h"
#include "hexview.h"

#include <QApplication>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString hex(quint64 v, int digits = 8)
{
    return QStringLiteral("0x") + QString::number(v, 16).toUpper().rightJustified(digits, QLatin1Char('0'));
}

QLabel *makeInfoLabel(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setContentsMargins(6, 3, 6, 3);
    l->setWordWrap(true);
    l->setAutoFillBackground(true);
    QPalette p = l->palette();
    p.setColor(QPalette::Window, p.color(QPalette::AlternateBase));
    l->setPalette(p);
    return l;
}

} // namespace

// ===========================================================================
// EditorPage

EditorPage::EditorPage(Document *doc, QWidget *parent)
    : TabPage(parent), m_doc(doc)
{
    doc->setParent(this);
    m_view = new HexView(this);
    m_view->setDocument(doc);
    m_info = makeInfoLabel(this);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_info);
    lay->addWidget(m_view, 1);

    connect(doc, &Document::modificationChanged, this, &TabPage::titleChanged);
    connect(doc, &Document::filePathChanged, this, [this]() {
        refreshInfo();
        emit titleChanged();
    });
    connect(doc, &Document::baseAddressChanged, this, &EditorPage::refreshInfo);
    connect(doc, &Document::contentsChanged, this, [this](qint64, qint64 removed, qint64 added) {
        if (removed != added)
            refreshInfo();
    });
    refreshInfo();
}

QList<HexView *> EditorPage::views() const
{
    return { m_view };
}

QString EditorPage::title() const
{
    return m_doc->displayName() + (m_doc->isModified() ? QStringLiteral(" *") : QString());
}

QString EditorPage::toolTip() const
{
    return m_doc->filePath().isEmpty() ? m_doc->displayName() : QDir::toNativeSeparators(m_doc->filePath());
}

void EditorPage::refreshInfo()
{
    if (m_doc->format() == FileFormat::Binary && m_doc->baseAddress() == 0) {
        m_info->hide();
        return;
    }
    const auto &fi = m_doc->formatInfo();
    QStringList parts;
    parts << Formats::formatName(m_doc->format());
    if (m_doc->format() != FileFormat::Binary)
        parts << tr("%n segment(s)", nullptr, int(fi.chunks.size()));
    if (m_doc->size() > 0)
        parts << tr("Address %1 - %2").arg(hex(m_doc->baseAddress()),
                                           hex(m_doc->baseAddress() + quint64(m_doc->size()) - 1));
    if (m_doc->hasGaps())
        parts << tr("gaps shown as 0x%1 (shaded)").arg(hex(m_doc->fillByte(), 2).mid(2));
    if (fi.hasStart)
        parts << tr("start address %1").arg(hex(fi.startAddress));
    m_info->setText(parts.join(QStringLiteral("  ·  ")));
    m_info->show();
}

// ===========================================================================
// ComparePage

ComparePage::ComparePage(Document *left, Document *right, QWidget *parent)
    : TabPage(parent), m_left(left), m_right(right)
{
    left->setParent(this);
    right->setParent(this);
    left->setReadOnly(true);
    right->setReadOnly(true);

    m_lview = new HexView(this);
    m_rview = new HexView(this);
    m_lview->setDocument(left);
    m_rview->setDocument(right);
    m_lview->setCompareBuffer(right->data());
    m_rview->setCompareBuffer(left->data());
    m_active = m_lview;

    auto makePanel = [this](Document *d, HexView *v) {
        auto *w = new QWidget(this);
        auto *l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(0);
        QLabel *cap = makeInfoLabel(w);
        QString txt = QStringLiteral("<b>%1</b>  ·  %2").arg(d->displayName().toHtmlEscaped(),
                                                            tr("%1 bytes").arg(QLocale().toString(d->size())));
        if (d->baseAddress())
            txt += QStringLiteral("  ·  ") + tr("base %1").arg(hex(d->baseAddress()));
        cap->setText(txt);
        cap->setToolTip(QDir::toNativeSeparators(d->filePath()));
        l->addWidget(cap);
        l->addWidget(v, 1);
        return w;
    };

    auto *hsplit = new QSplitter(Qt::Horizontal, this);
    hsplit->addWidget(makePanel(left, m_lview));
    hsplit->addWidget(makePanel(right, m_rview));

    m_list = new QTreeWidget(this);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setHeaderLabels({ tr("#"), tr("Offset"), tr("Length"), tr("Left address"), tr("Right address") });
    m_list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto *vsplit = new QSplitter(Qt::Vertical, this);
    vsplit->addWidget(hsplit);
    vsplit->addWidget(m_list);
    vsplit->setStretchFactor(0, 4);
    vsplit->setStretchFactor(1, 1);

    m_summary = makeInfoLabel(this);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_summary);
    lay->addWidget(vsplit, 1);

    // keep both views scrolled and positioned together
    auto syncScroll = [this](HexView *from, HexView *to) {
        connect(from->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, to](int v) {
            if (m_syncing)
                return;
            m_syncing = true;
            to->verticalScrollBar()->setValue(v);
            m_syncing = false;
        });
        connect(from->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this, to](int v) {
            if (m_syncing)
                return;
            m_syncing = true;
            to->horizontalScrollBar()->setValue(v);
            m_syncing = false;
        });
        connect(from, &HexView::cursorPositionChanged, this, [this, from, to](qint64 pos) {
            if (m_syncing || !from->hasFocus())
                return;
            m_syncing = true;
            to->setCursorPosition(pos);
            m_syncing = false;
        });
    };
    syncScroll(m_lview, m_rview);
    syncScroll(m_rview, m_lview);

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if ((now == m_lview || now == m_rview) && now != m_active) {
            m_active = static_cast<HexView *>(now);
            emit activeViewChanged(m_active);
        }
    });
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *it) {
        jumpTo(m_list->indexOfTopLevelItem(it));
    });
    connect(m_list, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *it) {
        jumpTo(m_list->indexOfTopLevelItem(it));
    });

    computeDiffs();
}

QList<HexView *> ComparePage::views() const
{
    return { m_lview, m_rview };
}

HexView *ComparePage::currentView() const
{
    return m_active;
}

QString ComparePage::title() const
{
    return tr("Compare: %1 ↔ %2").arg(m_left->displayName(), m_right->displayName());
}

QString ComparePage::toolTip() const
{
    return QDir::toNativeSeparators(m_left->filePath()) + QStringLiteral("\n↔ ")
           + QDir::toNativeSeparators(m_right->filePath());
}

void ComparePage::computeDiffs()
{
    constexpr int kMaxListed = 20000;
    const QByteArray &a = m_left->data();
    const QByteArray &b = m_right->data();
    const qint64 n = qMin(a.size(), b.size());
    m_diffs.clear();
    m_diffBytes = 0;
    m_truncated = false;

    const char *pa = a.constData();
    const char *pb = b.constData();
    qint64 i = 0;
    while (i < n) {
        if (pa[i] == pb[i]) {
            ++i;
            continue;
        }
        const qint64 start = i;
        while (i < n && pa[i] != pb[i])
            ++i;
        m_diffBytes += i - start;
        if (m_diffs.size() < kMaxListed)
            m_diffs.append({ start, i - start });
        else
            m_truncated = true;
    }
    const qint64 tail = qAbs(a.size() - b.size());
    if (tail > 0) {
        m_diffBytes += tail;
        if (m_diffs.size() < kMaxListed)
            m_diffs.append({ n, tail });
        else
            m_truncated = true;
    }

    m_list->setUpdatesEnabled(false);
    m_list->clear();
    QList<QTreeWidgetItem *> items;
    items.reserve(m_diffs.size());
    for (int k = 0; k < m_diffs.size(); ++k) {
        const auto &d = m_diffs[k];
        auto *it = new QTreeWidgetItem;
        it->setText(0, QString::number(k + 1));
        it->setText(1, hex(quint64(d.first)));
        it->setText(2, QStringLiteral("%1 (%2)").arg(d.second).arg(hex(quint64(d.second), 1)));
        it->setText(3, d.first < a.size() ? hex(m_left->baseAddress() + quint64(d.first)) : QStringLiteral("—"));
        it->setText(4, d.first < b.size() ? hex(m_right->baseAddress() + quint64(d.first)) : QStringLiteral("—"));
        items.append(it);
    }
    m_list->addTopLevelItems(items);
    m_list->setUpdatesEnabled(true);

    QString s;
    if (m_diffBytes == 0) {
        s = tr("Files are identical (%1 bytes).").arg(QLocale().toString(a.size()));
    } else {
        s = tr("%1 differing byte(s) in %2 block(s).")
                .arg(QLocale().toString(m_diffBytes))
                .arg(m_truncated ? tr("more than %1").arg(kMaxListed) : QString::number(m_diffs.size()));
        if (a.size() != b.size())
            s += QLatin1Char(' ') + tr("Sizes differ: %1 vs %2 bytes.")
                                        .arg(QLocale().toString(a.size()), QLocale().toString(b.size()));
        s += QLatin1Char(' ') + tr("F8 / Shift+F8: next / previous difference.");
    }
    if (m_left->baseAddress() != m_right->baseAddress())
        s += QLatin1Char(' ') + tr("Note: base addresses differ; bytes are compared by offset.");
    m_summary->setText(s);
}

int ComparePage::indexAt(qint64 pos, bool forward) const
{
    auto cmp = [](const QPair<qint64, qint64> &d, qint64 p) { return d.first < p; };
    if (forward) {
        auto it = std::lower_bound(m_diffs.cbegin(), m_diffs.cend(), pos + 1, cmp);
        return it == m_diffs.cend() ? -1 : int(it - m_diffs.cbegin());
    }
    auto it = std::lower_bound(m_diffs.cbegin(), m_diffs.cend(), pos, cmp);
    return it == m_diffs.cbegin() ? -1 : int(it - m_diffs.cbegin()) - 1;
}

void ComparePage::jumpTo(int index)
{
    if (index < 0 || index >= m_diffs.size())
        return;
    const auto &d = m_diffs[index];
    m_syncing = true;
    for (HexView *v : { m_lview, m_rview }) {
        const qint64 size = v->document()->size();
        if (d.first < size)
            v->setSelection(d.first, qMin(d.second, size - d.first));
        else
            v->setCursorPosition(size);
    }
    m_syncing = false;
    m_list->setCurrentItem(m_list->topLevelItem(index));
    m_list->scrollToItem(m_list->topLevelItem(index));
}

void ComparePage::nextDifference()
{
    const int i = indexAt(m_active->cursorPosition(), true);
    if (i >= 0)
        jumpTo(i);
    else
        QApplication::beep();
}

void ComparePage::previousDifference()
{
    const int i = indexAt(m_active->cursorPosition(), false);
    if (i >= 0)
        jumpTo(i);
    else
        QApplication::beep();
}
