// qBinEdit - in-memory document with undo/redo
#pragma once

#include "formats.h"

#include <QByteArray>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class QUndoStack;

inline qint64 bound64(qint64 lo, qint64 v, qint64 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

class Document : public QObject
{
    Q_OBJECT
public:
    explicit Document(QObject *parent = nullptr);

    // Load from disk. forceFormat: use Binary to open any file as raw bytes,
    // or pass -1 (default) for automatic detection.
    bool load(const QString &path, int forceFormat, QString *error, QStringList *warnings);
    bool save(const QString &path, FileFormat format, QString *error);
    void setContent(const QByteArray &data, quint64 baseAddress, const QString &displayName);

    const QByteArray &data() const { return m_data; }
    qint64 size() const { return m_data.size(); }
    quint8 byteAt(qint64 pos) const { return quint8(m_data.at(pos)); }
    QByteArray bytes(qint64 pos, qint64 len) const { return m_data.mid(pos, len); }

    quint64 baseAddress() const { return m_base; }
    void setBaseAddress(quint64 base);

    QString filePath() const { return m_path; }
    QString displayName() const;
    FileFormat format() const { return m_format; }
    const Formats::ParseResult &formatInfo() const { return m_info; }
    quint8 fillByte() const { return m_fill; }

    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool ro);
    bool isModified() const;

    QUndoStack *undoStack() const { return m_undo; }

    // Highlight helpers for the view.
    bool isChangedByte(qint64 pos) const;
    bool isGap(qint64 pos) const;
    bool hasGaps() const { return !m_gapMask.isEmpty(); }

    // Undoable edit: replace [pos, pos+len) with bytes. len==bytes.size() overwrites,
    // len==0 inserts, bytes empty deletes. "mergeable" coalesces consecutive typing.
    void replace(qint64 pos, qint64 len, const QByteArray &bytes, const QString &text, bool mergeable = false);

    // Undoable edit as ONE step: replace len bytes at each of the ascending, non-overlapping
    // positions with 'bytes'. Runs in a single pass over the data. Returns the number of
    // positions whose bytes actually changed.
    int replaceAll(const QVector<qint64> &positions, qint64 len, const QByteArray &bytes, const QString &text);

    // Change tracking used to highlight edited bytes. While only overwrites are in effect the
    // view compares against the loaded bytes; once bytes were inserted or deleted, edited
    // ranges are tracked instead so highlighting survives the shift.
    struct ChangeState {
        QVector<QPair<qint64, qint64>> ranges;   // sorted, merged [start, end) of edited bytes
        bool shifted = false;                    // an insert/delete is in effect
        int epoch = 0;                           // baseline (load/save) the state refers to
    };
    const ChangeState &changeState() const { return m_changes; }
    void restoreChangeState(const ChangeState &s);

    // Used by the undo commands only.
    void rawReplace(qint64 pos, qint64 len, const QByteArray &bytes);
    // Replace 'removed' bytes at each ascending position (current coordinates) in one pass.
    // bytes holds one chunk for all positions, or one chunk per position when perPosition.
    // maskBytes (same layout as bytes, optional) restores gap-mask values of the new bytes.
    void rawReplaceMany(const QVector<qint64> &positions, qint64 removed, const QByteArray &bytes,
                        bool perPosition, const QByteArray &maskBytes);
    const QByteArray &gapMask() const { return m_gapMask; }
    void notifyCursorHint(qint64 pos) { emit cursorHint(pos); }

signals:
    void contentsChanged(qint64 pos, qint64 removed, qint64 added);
    void modificationChanged(bool modified);
    void readOnlyChanged(bool readOnly);
    void cursorHint(qint64 pos);
    void filePathChanged();
    void baseAddressChanged();

private:
    void resetChanges();
    // Positions are ascending and in the coordinates before the edit; each replaces
    // 'removed' bytes by 'added' bytes.
    void trackEdits(const QVector<qint64> &positions, qint64 removed, qint64 added);

    QByteArray m_data;
    QByteArray m_original;      // snapshot at load/save, for change highlighting
    QByteArray m_gapMask;       // HEX/S-Record: 1 = byte not present in file
    quint64 m_base = 0;
    quint8 m_fill = 0xFF;
    QString m_path;
    QString m_name;
    FileFormat m_format = FileFormat::Binary;
    Formats::ParseResult m_info;
    bool m_readOnly = true;
    QUndoStack *m_undo = nullptr;
    ChangeState m_changes;
};
