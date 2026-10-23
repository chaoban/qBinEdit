// qBinEdit - hex / octal / binary view widget
#pragma once

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QFont>
#include <QPointer>
#include <QVector>

class Document;

class HexView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit HexView(QWidget *parent = nullptr);

    void setDocument(Document *doc);
    Document *document() const { return m_doc; }

    // --- display options --------------------------------------------------
    int radix() const { return m_radix; }              // 2, 8 or 16
    void setRadix(int radix);
    int unitSize() const { return m_unit; }            // 1, 2 or 4 bytes (8/16/32-bit)
    void setUnitSize(int bytes);
    bool bigEndian() const { return m_bigEndian; }
    void setBigEndian(bool big);
    int bytesPerRow() const { return m_bpr; }
    void setBytesPerRow(int n);
    bool showAscii() const { return m_showAscii; }
    void setShowAscii(bool on);
    bool dimFillBytes() const { return m_dimFill; }
    void setDimFillBytes(bool on);
    QFont viewFont() const { return m_font; }
    void setViewFont(const QFont &f);
    void zoom(int steps);   // +1 / -1, 0 = reset

    // --- cursor / selection -----------------------------------------------
    qint64 cursorPosition() const { return m_cursor; }
    void setCursorPosition(qint64 pos);
    bool hasSelection() const;
    qint64 selectionStart() const;
    qint64 selectionLength() const;
    void setSelection(qint64 start, qint64 length);
    void selectAll();
    void clearSelection();
    QByteArray selectedBytes() const;
    bool asciiPaneActive() const { return m_asciiPane; }

    // --- highlights -------------------------------------------------------
    void setCompareBuffer(const QByteArray &other);     // bytes differing from 'other' are highlighted
    void setFoundRange(qint64 start, qint64 length);

    qint64 topRow() const;
    void setTopRow(qint64 row);
    int visibleRows() const;

    // Value of the unit starting at offset, using current width and byte order.
    quint64 unitValue(qint64 offset, int *available = nullptr) const;
    QString formatValue(quint64 v, int unitBytes, int radix) const;

signals:
    void cursorPositionChanged(qint64 pos);
    void selectionChanged();
    void statusMessage(const QString &msg);
    void contextMenuRequested(const QPoint &globalPos);
    void zoomChanged();

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void changeEvent(QEvent *e) override;
    bool focusNextPrevChild(bool) override { return false; }   // Tab switches panes
    void scrollContentsBy(int dx, int dy) override;

private:
    struct Hit {
        bool valid = false;
        bool ascii = false;
        qint64 offset = 0;
        int digit = 0;
    };

    void recalcLayout();
    void updateScrollBars();
    int digitsPerUnit() const;
    int unitsPerRow() const { return m_bpr / m_unit; }
    qint64 maxCursor() const;
    qint64 alignDown(qint64 pos) const { return pos - (pos % m_unit); }
    int granularity() const { return m_asciiPane ? 1 : m_unit; }
    Hit hitTest(const QPoint &p, bool clamp) const;
    void moveCursor(qint64 pos, int digit, bool select);
    void ensureCursorVisible();
    bool checkEditable();
    void typeDigit(int value);
    void typeAscii(char c);
    QByteArray encodeUnit(quint64 v) const;
    bool unitDiffers(qint64 off, int len) const;
    QRect cursorRect() const;

    QPointer<Document> m_doc;
    int m_radix = 16;
    int m_unit = 1;
    int m_bpr = 16;
    bool m_bigEndian = false;
    bool m_showAscii = true;
    bool m_dimFill = true;

    qint64 m_cursor = 0;
    int m_digit = 0;
    bool m_asciiPane = false;
    qint64 m_anchor = 0;
    bool m_hasSel = false;
    bool m_mouseDown = false;
    bool m_inEdit = false;

    QFont m_font;
    qreal m_basePointSize = 10;
    int m_charW = 8;
    int m_lineH = 16;
    int m_ascent = 12;
    int m_headerH = 20;
    int m_addrDigits = 8;
    int m_xAddr = 4;
    int m_xData = 0;
    int m_xAscii = 0;
    int m_dataW = 0;
    int m_contentW = 0;
    QVector<int> m_unitX;

    QByteArray m_compare;
    bool m_hasCompare = false;
    qint64 m_foundStart = -1;
    qint64 m_foundLen = 0;
};
