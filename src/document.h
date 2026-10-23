// qBinEdit - in-memory document with undo/redo
#pragma once

#include "formats.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

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

    // Used by the undo command only.
    void rawReplace(qint64 pos, qint64 len, const QByteArray &bytes);
    void notifyCursorHint(qint64 pos) { emit cursorHint(pos); }

signals:
    void contentsChanged(qint64 pos, qint64 removed, qint64 added);
    void modificationChanged(bool modified);
    void readOnlyChanged(bool readOnly);
    void cursorHint(qint64 pos);
    void filePathChanged();
    void baseAddressChanged();

private:
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
};
