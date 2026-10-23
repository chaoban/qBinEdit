// qBinEdit - hex / octal / binary view widget
#include "hexview.h"
#include "document.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <limits>

namespace {

QFont defaultMonoFont()
{
#ifdef Q_OS_WIN
    QFont f(QStringLiteral("Consolas"), 11);
    f.setStyleHint(QFont::Monospace);
    if (!QFontInfo(f).fixedPitch()) {
        f = QFontDatabase::systemFont(QFontDatabase::FixedFont);   // e.g. Courier New
        f.setPointSize(11);
    }
#else
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (f.pointSizeF() < 10)
        f.setPointSizeF(10);
#endif
    f.setFixedPitch(true);
    f.setKerning(false);
    return f;
}

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(float(a.redF() * (1 - t) + b.redF() * t),
                            float(a.greenF() * (1 - t) + b.greenF() * t),
                            float(a.blueF() * (1 - t) + b.blueF() * t));
}

} // namespace

HexView::HexView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    m_font = defaultMonoFont();
    m_basePointSize = m_font.pointSizeF();
    verticalScrollBar()->setSingleStep(1);
    recalcLayout();
}

void HexView::setDocument(Document *doc)
{
    if (m_doc)
        disconnect(m_doc, nullptr, this, nullptr);
    m_doc = doc;
    m_cursor = 0;
    m_anchor = 0;
    m_digit = 0;
    m_hasSel = false;
    if (doc) {
        connect(doc, &Document::contentsChanged, this, [this](qint64, qint64, qint64) {
            recalcLayout();
            if (m_cursor > maxCursor())
                m_cursor = maxCursor();
            viewport()->update();
            emit cursorPositionChanged(m_cursor);
        });
        connect(doc, &Document::readOnlyChanged, this, [this](bool) {
            recalcLayout();
            if (m_cursor > maxCursor())
                moveCursor(maxCursor(), 0, false);
            viewport()->update();
        });
        connect(doc, &Document::baseAddressChanged, this, [this]() {
            recalcLayout();
            viewport()->update();
            emit cursorPositionChanged(m_cursor);
        });
        connect(doc, &Document::cursorHint, this, [this](qint64 pos) {
            if (!m_inEdit)
                moveCursor(qMin(pos, maxCursor()), 0, false);
        });
    }
    recalcLayout();
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
    emit selectionChanged();
}

// ---------------------------------------------------------------------------
// options

void HexView::setRadix(int radix)
{
    if (radix != 2 && radix != 8 && radix != 16)
        radix = 16;
    m_radix = radix;
    m_digit = 0;
    recalcLayout();
    ensureCursorVisible();
    viewport()->update();
}

void HexView::setUnitSize(int bytes)
{
    if (bytes != 1 && bytes != 2 && bytes != 4)
        bytes = 1;
    m_unit = bytes;
    if (m_bpr % m_unit)
        m_bpr = 16;
    m_digit = 0;
    if (!m_asciiPane)
        m_cursor = alignDown(m_cursor);
    recalcLayout();
    ensureCursorVisible();
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
}

void HexView::setBigEndian(bool big)
{
    m_bigEndian = big;
    viewport()->update();
}

void HexView::setBytesPerRow(int n)
{
    n = qBound(4, n, 64);
    if (n % m_unit)
        n = 16;
    const qint64 keep = m_cursor;
    m_bpr = n;
    recalcLayout();
    m_cursor = keep;
    ensureCursorVisible();
    viewport()->update();
}

void HexView::setShowAscii(bool on)
{
    m_showAscii = on;
    if (!on && m_asciiPane) {
        m_asciiPane = false;
        m_cursor = alignDown(m_cursor);
    }
    recalcLayout();
    viewport()->update();
}

void HexView::setDimFillBytes(bool on)
{
    m_dimFill = on;
    viewport()->update();
}

void HexView::setViewFont(const QFont &f)
{
    m_font = f;
    m_font.setFixedPitch(true);
    m_font.setKerning(false);
    m_basePointSize = m_font.pointSizeF() > 0 ? m_font.pointSizeF() : 10;
    recalcLayout();
    viewport()->update();
    emit zoomChanged();
}

void HexView::zoom(int steps)
{
    qreal size = m_font.pointSizeF();
    if (steps == 0)
        size = m_basePointSize;
    else
        size = qBound<qreal>(6, size + steps, 48);
    m_font.setPointSizeF(size);
    recalcLayout();
    ensureCursorVisible();
    viewport()->update();
    emit zoomChanged();
}

// ---------------------------------------------------------------------------
// geometry

int HexView::digitsPerUnit() const
{
    const int bits = 8 * m_unit;
    switch (m_radix) {
    case 2:  return bits;
    case 8:  return (bits + 2) / 3;
    default: return bits / 4;
    }
}

qint64 HexView::maxCursor() const
{
    if (!m_doc)
        return 0;
    const qint64 size = m_doc->size();
    return m_doc->isReadOnly() ? qMax<qint64>(0, size - 1) : size;
}

void HexView::recalcLayout()
{
    const QFontMetrics fm(m_font);
    m_charW = qMax(1, fm.horizontalAdvance(QLatin1Char('0')));
    m_lineH = fm.height() + 3;
    m_ascent = fm.ascent() + 1;
    m_headerH = m_lineH + 4;

    quint64 maxAddr = 0;
    if (m_doc)
        maxAddr = m_doc->baseAddress() + quint64(m_doc->size());
    m_addrDigits = (maxAddr > 0xFFFFFFFFULL) ? ((maxAddr > 0xFFFFFFFFFFFFULL) ? 16 : 12) : 8;

    m_xAddr = m_charW;
    m_xData = m_xAddr + (m_addrDigits + 2) * m_charW;

    const int d = digitsPerUnit();
    const int upr = unitsPerRow();
    m_unitX.resize(upr);
    for (int u = 0; u < upr; ++u) {
        const int byteIdx = u * m_unit;
        const int groupGaps = (m_bpr > 8 && m_unit < 8) ? byteIdx / 8 : 0;
        m_unitX[u] = m_xData + u * (d + 1) * m_charW + groupGaps * m_charW;
    }
    m_dataW = (upr > 0 ? m_unitX.last() + d * m_charW : m_xData) - m_xData;
    m_xAscii = m_xData + m_dataW + 2 * m_charW;
    m_contentW = (m_showAscii ? m_xAscii + m_bpr * m_charW : m_xData + m_dataW) + m_charW;
    updateScrollBars();
}

int HexView::visibleRows() const
{
    return qMax(1, (viewport()->height() - m_headerH) / m_lineH);
}

void HexView::updateScrollBars()
{
    const qint64 rowsTotal = maxCursor() / m_bpr + 1;
    const int vis = visibleRows();
    QScrollBar *vb = verticalScrollBar();
    vb->setRange(0, int(qMin<qint64>(qMax<qint64>(0, rowsTotal - vis), std::numeric_limits<int>::max())));
    vb->setPageStep(vis);
    QScrollBar *hb = horizontalScrollBar();
    hb->setRange(0, qMax(0, m_contentW - viewport()->width()));
    hb->setPageStep(viewport()->width());
    hb->setSingleStep(m_charW * 2);
}

qint64 HexView::topRow() const
{
    return verticalScrollBar()->value();
}

void HexView::setTopRow(qint64 row)
{
    verticalScrollBar()->setValue(int(bound64(0, row, verticalScrollBar()->maximum())));
}

void HexView::resizeEvent(QResizeEvent *e)
{
    QAbstractScrollArea::resizeEvent(e);
    updateScrollBars();
}

void HexView::scrollContentsBy(int, int)
{
    viewport()->update();
}

// ---------------------------------------------------------------------------
// value helpers

quint64 HexView::unitValue(qint64 offset, int *available) const
{
    if (!m_doc)
        return 0;
    const qint64 size = m_doc->size();
    const int avail = int(bound64(0, size - offset, m_unit));
    const QByteArray &d = m_doc->data();
    quint64 v = 0;
    if (!m_bigEndian) {
        for (int i = 0; i < avail; ++i)
            v |= quint64(uchar(d.at(offset + i))) << (8 * i);
    } else {
        for (int i = 0; i < avail; ++i)
            v = (v << 8) | uchar(d.at(offset + i));
        v <<= 8 * (m_unit - avail);
    }
    if (available)
        *available = avail;
    return v;
}

QString HexView::formatValue(quint64 v, int unitBytes, int radix) const
{
    const int bits = 8 * unitBytes;
    const int digits = radix == 2 ? bits : (radix == 8 ? (bits + 2) / 3 : bits / 4);
    return QString::number(v, radix).toUpper().rightJustified(digits, QLatin1Char('0'));
}

QByteArray HexView::encodeUnit(quint64 v) const
{
    QByteArray b(m_unit, '\0');
    for (int i = 0; i < m_unit; ++i)
        b[m_bigEndian ? (m_unit - 1 - i) : i] = char((v >> (8 * i)) & 0xFF);
    return b;
}

bool HexView::unitDiffers(qint64 off, int len) const
{
    const QByteArray &d = m_doc->data();
    for (int i = 0; i < len; ++i) {
        const qint64 o = off + i;
        if (o >= m_compare.size() || m_compare.at(o) != d.at(o))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// cursor & selection

void HexView::setCursorPosition(qint64 pos)
{
    moveCursor(pos, 0, false);
}

bool HexView::hasSelection() const
{
    return m_hasSel && m_doc && m_doc->size() > 0;
}

qint64 HexView::selectionStart() const
{
    const int g = granularity();
    qint64 a = qMin(m_anchor, m_cursor);
    return a - a % g;
}

qint64 HexView::selectionLength() const
{
    if (!hasSelection())
        return 0;
    const int g = granularity();
    qint64 b = qMax(m_anchor, m_cursor);
    b = b - b % g + g;
    b = qMin(b, m_doc->size());
    return qMax<qint64>(0, b - selectionStart());
}

void HexView::setSelection(qint64 start, qint64 length)
{
    if (!m_doc || length <= 0) {
        clearSelection();
        return;
    }
    start = bound64(0, start, qMax<qint64>(0, m_doc->size() - 1));
    m_anchor = qMin(start + length - 1, m_doc->size() - 1);
    m_cursor = start;
    m_digit = 0;
    m_hasSel = true;
    ensureCursorVisible();
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
    emit selectionChanged();
}

void HexView::selectAll()
{
    if (!m_doc || m_doc->size() == 0)
        return;
    m_anchor = m_doc->size() - 1;
    m_cursor = 0;
    m_digit = 0;
    m_hasSel = true;
    ensureCursorVisible();
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
    emit selectionChanged();
}

void HexView::clearSelection()
{
    const bool had = m_hasSel;
    m_hasSel = false;
    m_anchor = m_cursor;
    viewport()->update();
    if (had)
        emit selectionChanged();
}

QByteArray HexView::selectedBytes() const
{
    if (!hasSelection())
        return QByteArray();
    return m_doc->bytes(selectionStart(), selectionLength());
}

void HexView::setCompareBuffer(const QByteArray &other)
{
    m_compare = other;
    m_hasCompare = true;
    viewport()->update();
}

void HexView::setFoundRange(qint64 start, qint64 length)
{
    m_foundStart = length > 0 ? start : -1;
    m_foundLen = length;
    viewport()->update();
}

void HexView::moveCursor(qint64 pos, int digit, bool select)
{
    const bool hadSel = m_hasSel;
    pos = bound64(0, pos, maxCursor());
    if (!m_asciiPane)
        pos = alignDown(pos);
    if (select) {
        if (!m_hasSel) {
            m_anchor = m_cursor;
            m_hasSel = true;
        }
    } else {
        m_hasSel = false;
        m_anchor = pos;
    }
    const bool moved = pos != m_cursor;
    m_cursor = pos;
    m_digit = qBound(0, digit, digitsPerUnit() - 1);
    ensureCursorVisible();
    viewport()->update();
    if (moved)
        emit cursorPositionChanged(m_cursor);
    if (select || hadSel)
        emit selectionChanged();
}

QRect HexView::cursorRect() const
{
    const qint64 row = m_cursor / m_bpr;
    const int y = m_headerH + int(row - topRow()) * m_lineH;
    if (m_asciiPane) {
        const int col = int(m_cursor % m_bpr);
        return QRect(m_xAscii + col * m_charW, y, m_charW, m_lineH);
    }
    const int u = int((alignDown(m_cursor) % m_bpr) / m_unit);
    return QRect(m_unitX.value(u) + m_digit * m_charW, y, m_charW, m_lineH);
}

void HexView::ensureCursorVisible()
{
    const qint64 row = m_cursor / m_bpr;
    const qint64 top = topRow();
    const int vis = visibleRows();
    if (row < top)
        setTopRow(row);
    else if (row >= top + vis)
        setTopRow(row - vis + 1);

    if (!isVisible() || viewport()->width() < 4 * m_charW)
        return;     // not laid out yet: never scroll horizontally on a guess
    const QRect r = cursorRect();
    QScrollBar *hb = horizontalScrollBar();
    const int hx = hb->value();
    const int vw = viewport()->width();
    if (r.right() + m_charW <= vw)
        hb->setValue(0);                               // keep the address column in view when possible
    else if (r.left() - m_charW < hx)
        hb->setValue(qMax(0, r.left() - 4 * m_charW));
    else if (r.right() + m_charW > hx + vw)
        hb->setValue(r.right() + 4 * m_charW - vw);
}

// ---------------------------------------------------------------------------
// painting

void HexView::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());
    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::Text);
    const bool dark = base.lightness() < 128;
    const QColor dim = mix(text, base, 0.55);
    const QColor grid = mix(text, base, 0.82);
    const QColor addrColor = dark ? QColor(0x8A, 0xB4, 0xF8) : QColor(0x1A, 0x5F, 0xB4);
    const QColor headerBg = mix(base, text, dark ? 0.08 : 0.05);
    const QColor selBg = pal.color(QPalette::Highlight);
    const QColor selFg = pal.color(QPalette::HighlightedText);
    const QColor changedFg = dark ? QColor(0xFF, 0x6B, 0x6B) : QColor(0xD3, 0x2F, 0x2F);
    const QColor diffBg = dark ? QColor(0x7A, 0x3E, 0x00) : QColor(0xFF, 0xD2, 0xA6);
    const QColor foundBg = dark ? QColor(0x66, 0x5A, 0x00) : QColor(0xFF, 0xF1, 0x76);
    const QColor gapBg = mix(base, text, dark ? 0.04 : 0.035);

    p.fillRect(viewport()->rect(), base);
    p.setFont(m_font);
    const int hx = horizontalScrollBar()->value();
    p.translate(-hx, 0);
    const int vw = viewport()->width() + hx;
    const int vh = viewport()->height();

    // address column + header backgrounds and separators
    p.fillRect(QRect(0, 0, m_xData - m_charW, vh), headerBg);
    p.fillRect(QRect(0, 0, qMax(vw, m_contentW), m_headerH), headerBg);
    p.setPen(grid);
    p.drawLine(m_xData - m_charW, 0, m_xData - m_charW, vh);
    p.drawLine(0, m_headerH - 1, qMax(vw, m_contentW), m_headerH - 1);
    if (m_showAscii)
        p.drawLine(m_xAscii - m_charW, 0, m_xAscii - m_charW, vh);

    const int headerBase = (m_headerH - m_lineH) / 2 + m_ascent;
    p.setPen(dim);
    const bool hasBase = m_doc && m_doc->baseAddress() != 0;
    p.drawText(QPoint(m_xAddr, headerBase), hasBase ? tr("Address") : tr("Offset"));
    const int d = digitsPerUnit();
    const int upr = unitsPerRow();
    for (int u = 0; u < upr; ++u) {
        const QString label = QString::number(u * m_unit, 16).toUpper().rightJustified(2, QLatin1Char('0'));
        const int lx = m_unitX[u] + qMax(0, (d - label.size()) * m_charW / 2);
        p.drawText(QPoint(lx, headerBase), label);
    }
    if (m_showAscii) {
        QString hdr;
        for (int i = 0; i < m_bpr; ++i)
            hdr += QString::number(i % 16, 16).toUpper();
        p.drawText(QPoint(m_xAscii, headerBase), hdr);
    }

    if (!m_doc)
        return;

    p.setClipRect(QRect(0, m_headerH, qMax(vw, m_contentW), vh - m_headerH));

    const QByteArray &data = m_doc->data();
    const qint64 size = data.size();
    const qint64 top = topRow();
    const int rows = visibleRows() + 1;
    const qint64 mx = maxCursor();
    qint64 selS = -1, selE = -1;
    if (hasSelection()) {
        selS = selectionStart();
        selE = selS + selectionLength();
    }
    const quint64 maxV = (m_unit >= 8) ? ~quint64(0) : ((quint64(1) << (8 * m_unit)) - 1);
    QFont boldFont = m_font;
    boldFont.setBold(true);

    auto inFound = [&](qint64 off, int len) {
        return m_foundStart >= 0 && off < m_foundStart + m_foundLen && off + len > m_foundStart;
    };

    for (int r = 0; r < rows; ++r) {
        const qint64 row = top + r;
        const qint64 rowStart = row * m_bpr;
        if (rowStart > mx)
            break;
        const int y = m_headerH + r * m_lineH;
        const int baseline = y + m_ascent;

        p.setPen(addrColor);
        const quint64 addr = m_doc->baseAddress() + quint64(rowStart);
        p.drawText(QPoint(m_xAddr, baseline),
                   QString::number(addr, 16).toUpper().rightJustified(m_addrDigits, QLatin1Char('0')));

        // data pane
        for (int u = 0; u < upr; ++u) {
            const qint64 off = rowStart + qint64(u) * m_unit;
            if (off >= size)
                break;
            int avail = 0;
            const quint64 v = unitValue(off, &avail);
            const int x = m_unitX[u];
            const bool sel = selS >= 0 && off < selE && off + m_unit > selS;
            bool changed = false;
            bool allGap = m_doc->hasGaps();
            for (int i = 0; i < avail; ++i) {
                changed = changed || m_doc->isChangedByte(off + i);
                allGap = allGap && m_doc->isGap(off + i);
            }

            QColor bg;
            if (sel)
                bg = selBg;
            else if (m_hasCompare && unitDiffers(off, avail))
                bg = diffBg;
            else if (inFound(off, m_unit))
                bg = foundBg;
            else if (allGap)
                bg = gapBg;
            if (bg.isValid()) {
                int w = d * m_charW;
                if (u + 1 < upr) {
                    const qint64 noff = off + m_unit;
                    const bool nextSel = selS >= 0 && noff < selE && noff < size;
                    if ((sel && nextSel) || (!sel && bg == foundBg && inFound(noff, m_unit) && noff < size))
                        w = m_unitX[u + 1] - x;
                }
                p.fillRect(QRect(x, y, w, m_lineH), bg);
            }

            QColor fg = text;
            if (sel)
                fg = selFg;
            else if (changed)
                fg = changedFg;
            else if (avail < m_unit || allGap)
                fg = dim;
            else if (m_dimFill && (v == 0 || v == maxV))
                fg = dim;
            p.setPen(fg);
            if (changed)
                p.setFont(boldFont);
            p.drawText(QPoint(x, baseline), formatValue(v, m_unit, m_radix));
            if (changed)
                p.setFont(m_font);
        }

        // ASCII pane
        if (m_showAscii) {
            QString line;
            for (int i = 0; i < m_bpr; ++i) {
                const qint64 off = rowStart + i;
                if (off >= size)
                    break;
                const uchar c = uchar(data.at(off));
                const int x = m_xAscii + i * m_charW;
                const bool sel = selS >= 0 && off >= selS && off < selE;
                const bool changed = m_doc->isChangedByte(off);
                QColor bg;
                if (sel)
                    bg = selBg;
                else if (m_hasCompare && unitDiffers(off, 1))
                    bg = diffBg;
                else if (inFound(off, 1))
                    bg = foundBg;
                else if (m_doc->isGap(off))
                    bg = gapBg;
                if (bg.isValid())
                    p.fillRect(QRect(x, y, m_charW, m_lineH), bg);
                const bool printable = c >= 0x20 && c < 0x7F;
                p.setPen(sel ? selFg : (changed ? changedFg : (printable ? text : dim)));
                p.drawText(QPoint(x, baseline), QString(printable ? QChar(c) : QChar(u'.')));
            }
        }
    }

    // cursor
    const qint64 crow = m_cursor / m_bpr;
    if (crow >= top && crow < top + rows) {
        const QRect r = cursorRect();
        const bool focused = hasFocus();
        const int y = r.top();
        const qint64 cu = alignDown(m_cursor);
        const int col = int(cu % m_bpr);
        // rectangle of the counterpart in the inactive pane
        QRect other;
        if (m_asciiPane) {
            const int u = int((m_cursor % m_bpr) / m_unit);
            other = QRect(m_unitX.value(u), y, d * m_charW, m_lineH);
        } else if (m_showAscii) {
            other = QRect(m_xAscii + col * m_charW, y, m_unit * m_charW, m_lineH);
        }
        QColor outline = text;
        outline.setAlpha(150);
        p.setPen(outline);
        if (!other.isNull())
            p.drawRect(other.adjusted(0, 0, -1, -1));

        if (focused) {
            p.fillRect(r, text);
            p.setPen(base);
            QString ch;
            if (m_asciiPane) {
                if (m_cursor < size) {
                    const uchar c = uchar(data.at(m_cursor));
                    ch = (c >= 0x20 && c < 0x7F) ? QString(QChar(c)) : QStringLiteral(".");
                }
            } else if (cu < size) {
                const QString s = formatValue(unitValue(cu), m_unit, m_radix);
                ch = s.mid(m_digit, 1);
            }
            if (!ch.isEmpty())
                p.drawText(QPoint(r.left(), y + m_ascent), ch);
        } else {
            p.setPen(text);
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
    }
}

// ---------------------------------------------------------------------------
// input

bool HexView::checkEditable()
{
    if (!m_doc)
        return false;
    if (m_doc->isReadOnly()) {
        emit statusMessage(tr("Read-only. Press Ctrl+E to enable editing."));
        QApplication::beep();
        return false;
    }
    return true;
}

void HexView::typeDigit(int value)
{
    if (!checkEditable())
        return;
    const qint64 size = m_doc->size();
    const qint64 off = alignDown(m_cursor);
    const bool append = off >= size;
    quint64 v = 0;
    if (!append) {
        int avail = 0;
        v = unitValue(off, &avail);
        if (avail < m_unit) {
            emit statusMessage(tr("Incomplete %1-bit unit at end of file. Switch to 8-bit width to edit it.").arg(m_unit * 8));
            QApplication::beep();
            return;
        }
    }
    const int d = digitsPerUnit();
    quint64 weight = 1;
    for (int i = 0; i < d - 1 - m_digit; ++i)
        weight *= quint64(m_radix);
    const quint64 cur = (v / weight) % quint64(m_radix);
    const quint64 nv = v - cur * weight + quint64(value) * weight;
    const quint64 maxV = (m_unit >= 8) ? ~quint64(0) : ((quint64(1) << (8 * m_unit)) - 1);
    if (nv > maxV) {
        emit statusMessage(tr("Value out of range for a %1-bit unit.").arg(m_unit * 8));
        QApplication::beep();
        return;
    }
    m_inEdit = true;
    m_doc->replace(off, append ? 0 : m_unit, encodeUnit(nv), tr("Typing"), true);
    m_inEdit = false;
    if (m_digit < d - 1) {
        ++m_digit;
        m_cursor = off;
    } else {
        m_digit = 0;
        m_cursor = qMin(off + m_unit, alignDown(maxCursor()));
    }
    m_hasSel = false;
    m_anchor = m_cursor;
    ensureCursorVisible();
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
}

void HexView::typeAscii(char c)
{
    if (!checkEditable())
        return;
    const qint64 off = m_cursor;
    const bool append = off >= m_doc->size();
    m_inEdit = true;
    m_doc->replace(off, append ? 0 : 1, QByteArray(1, c), tr("Typing"), true);
    m_inEdit = false;
    m_cursor = qMin(off + 1, maxCursor());
    m_hasSel = false;
    m_anchor = m_cursor;
    ensureCursorVisible();
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
}

void HexView::keyPressEvent(QKeyEvent *e)
{
    if (!m_doc) {
        QAbstractScrollArea::keyPressEvent(e);
        return;
    }
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const int d = digitsPerUnit();
    const int g = granularity();
    const qint64 cur = m_asciiPane ? m_cursor : alignDown(m_cursor);
    const qint64 mx = m_asciiPane ? maxCursor() : alignDown(maxCursor());

    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_Backspace:
        if (m_asciiPane || shift || ctrl) {
            if (cur >= g)
                moveCursor(cur - g, 0, shift);
        } else if (m_digit > 0) {
            --m_digit;
            if (m_hasSel)
                clearSelection();
            ensureCursorVisible();
            viewport()->update();
        } else if (cur >= g) {
            moveCursor(cur - g, d - 1, false);
        }
        return;
    case Qt::Key_Right:
        if (m_asciiPane || shift || ctrl) {
            if (cur + g <= mx)
                moveCursor(cur + g, 0, shift);
        } else if (m_digit < d - 1) {
            ++m_digit;
            if (m_hasSel)
                clearSelection();
            ensureCursorVisible();
            viewport()->update();
        } else if (cur + g <= mx) {
            moveCursor(cur + g, 0, false);
        }
        return;
    case Qt::Key_Up:
        if (cur >= m_bpr)
            moveCursor(cur - m_bpr, m_digit, shift);
        return;
    case Qt::Key_Down:
        moveCursor(cur + m_bpr <= mx ? cur + m_bpr : mx, m_digit, shift);
        return;
    case Qt::Key_PageUp: {
        const int vis = visibleRows();
        const qint64 step = qint64(vis) * m_bpr;
        setTopRow(topRow() - vis);
        moveCursor(cur >= step ? cur - step : cur % m_bpr, m_digit, shift);
        return;
    }
    case Qt::Key_PageDown: {
        const int vis = visibleRows();
        const qint64 step = qint64(vis) * m_bpr;
        setTopRow(topRow() + vis);
        moveCursor(cur + step <= mx ? cur + step : mx, m_digit, shift);
        return;
    }
    case Qt::Key_Home:
        moveCursor(ctrl ? 0 : cur - cur % m_bpr, 0, shift);
        return;
    case Qt::Key_End:
        moveCursor(ctrl ? mx : qMin(cur - cur % m_bpr + m_bpr - g, mx), 0, shift);
        return;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        if (m_showAscii) {
            m_asciiPane = !m_asciiPane;
            if (!m_asciiPane)
                m_cursor = alignDown(m_cursor);
            m_digit = 0;
            ensureCursorVisible();
            viewport()->update();
        }
        return;
    case Qt::Key_Escape:
        clearSelection();
        return;
    default:
        break;
    }

    const QString t = e->text();
    if (!t.isEmpty() && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        const char16_t c = t.at(0).unicode();
        if (m_asciiPane) {
            if (c >= 0x20 && c < 0x7F) {
                typeAscii(char(c));
                return;
            }
        } else {
            int v = -1;
            if (c >= u'0' && c <= u'9')
                v = c - u'0';
            else if (c >= u'a' && c <= u'f')
                v = c - u'a' + 10;
            else if (c >= u'A' && c <= u'F')
                v = c - u'A' + 10;
            if (v >= 0 && v < m_radix) {
                typeDigit(v);
                return;
            }
        }
    }
    QAbstractScrollArea::keyPressEvent(e);
}

HexView::Hit HexView::hitTest(const QPoint &pt, bool clamp) const
{
    Hit h;
    if (!m_doc)
        return h;
    const int x = pt.x() + horizontalScrollBar()->value();
    const int y = pt.y();
    if (!clamp && y < m_headerH)
        return h;
    const int dy = y - m_headerH;
    qint64 row = topRow() + (dy >= 0 ? dy / m_lineH : -1 - (-dy - 1) / m_lineH);
    const qint64 mx = maxCursor();
    row = bound64(0, row, mx / m_bpr);

    if (m_showAscii && x >= m_xAscii - m_charW) {
        h.ascii = true;
        const int col = qBound(0, (x - m_xAscii) / m_charW, m_bpr - 1);
        h.offset = row * m_bpr + col;
        if (h.offset > mx)
            h.offset = mx;
    } else {
        int u = 0;
        for (int i = unitsPerRow() - 1; i >= 0; --i) {
            if (x >= m_unitX[i] - m_charW / 2) {
                u = i;
                break;
            }
        }
        h.digit = qBound(0, (x - m_unitX[u]) / m_charW, digitsPerUnit() - 1);
        h.offset = row * m_bpr + qint64(u) * m_unit;
        if (h.offset > mx) {
            h.offset = alignDown(mx);
            h.digit = 0;
        }
    }
    h.valid = true;
    return h;
}

void HexView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(e);
        return;
    }
    const Hit h = hitTest(e->position().toPoint(), true);
    if (!h.valid)
        return;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    if (shift) {
        if (!m_hasSel) {
            m_anchor = m_cursor;
            m_hasSel = true;
        }
    } else {
        m_hasSel = false;
    }
    m_asciiPane = h.ascii;
    m_cursor = h.ascii ? h.offset : alignDown(h.offset);
    m_digit = h.digit;
    if (!shift)
        m_anchor = m_cursor;
    m_mouseDown = true;
    viewport()->update();
    emit cursorPositionChanged(m_cursor);
    emit selectionChanged();
}

void HexView::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_mouseDown)
        return;
    const QPoint pt = e->position().toPoint();
    if (pt.y() < m_headerH)
        setTopRow(topRow() - 1);
    else if (pt.y() > viewport()->height())
        setTopRow(topRow() + 1);
    const Hit h = hitTest(pt, true);
    if (!h.valid)
        return;
    const qint64 pos = m_asciiPane ? h.offset : alignDown(h.offset);
    if (pos != m_cursor) {
        m_cursor = pos;
        m_digit = 0;
        m_hasSel = true;
        viewport()->update();
        emit cursorPositionChanged(m_cursor);
        emit selectionChanged();
    }
}

void HexView::mouseReleaseEvent(QMouseEvent *e)
{
    m_mouseDown = false;
    QAbstractScrollArea::mouseReleaseEvent(e);
}

void HexView::mouseDoubleClickEvent(QMouseEvent *e)
{
    // Double click selects the unit (data pane) or the byte (ASCII pane) under the mouse.
    const Hit h = hitTest(e->position().toPoint(), true);
    if (!h.valid || !m_doc || h.offset >= m_doc->size())
        return;
    m_asciiPane = h.ascii;
    const qint64 start = h.ascii ? h.offset : alignDown(h.offset);
    setSelection(start, h.ascii ? 1 : qMin<qint64>(m_unit, m_doc->size() - start));
}

void HexView::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        const int dy = e->angleDelta().y();
        if (dy != 0)
            zoom(dy > 0 ? 1 : -1);
        e->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(e);
}

void HexView::contextMenuEvent(QContextMenuEvent *e)
{
    emit contextMenuRequested(e->globalPos());
}

void HexView::focusInEvent(QFocusEvent *e)
{
    QAbstractScrollArea::focusInEvent(e);
    viewport()->update();
}

void HexView::focusOutEvent(QFocusEvent *e)
{
    QAbstractScrollArea::focusOutEvent(e);
    viewport()->update();
}

void HexView::changeEvent(QEvent *e)
{
    QAbstractScrollArea::changeEvent(e);
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange)
        viewport()->update();
}
