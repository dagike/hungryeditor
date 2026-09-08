#pragma once

#include <QByteArray>
#include <QString>

namespace hungryeditor {

/// How a file's bytes are decoded and re-encoded. Detected on load from a
/// byte-order mark or a strict UTF-8 check; preserved so a save round-trips
/// the original file unchanged.
enum class Encoding
{
    Utf8,
    Utf8Bom,
    Utf16Le,
    Utf16Be,
    Latin1,
};

/// The newline sequence a file uses. The in-memory text is always normalised
/// to "\n"; this records what to write back.
enum class LineEnding
{
    Lf,   ///< "\n"   — Unix
    CrLf, ///< "\r\n" — Windows
    Cr,   ///< "\r"   — classic Mac
};

/// A decoded text file: contents plus the encoding and line ending needed to
/// write it back exactly as it came in.
struct TextDocument
{
    QString text; ///< newlines normalised to "\n"
    Encoding encoding = Encoding::Utf8;
    LineEnding lineEnding = LineEnding::Lf;
};

/// Outcome of a load or save. `ok` is false only on an I/O failure; a
/// successful decode of otherwise odd bytes still reports `ok`.
struct FileError
{
    bool ok = true;
    QString message;
};

/// The literal sequence for a line ending.
QString lineEndingText(LineEnding eol);

/// Decode raw file bytes: strip a BOM, choose an encoding, detect the
/// dominant line ending and normalise every newline to "\n".
TextDocument decodeBytes(const QByteArray& bytes);

/// Re-encode a document: restore its line ending and BOM.
QByteArray encodeDocument(const TextDocument& doc);

/// Read and decode a file. On an I/O error `error` is filled and the returned
/// document is empty.
TextDocument loadFile(const QString& path, FileError* error = nullptr);

/// Encode and write a document atomically (write-temp-then-rename).
bool saveFile(const QString& path, const TextDocument& doc, FileError* error = nullptr);

} // namespace hungryeditor
