#pragma once

#include <QString>

class QImage;

namespace hungryeditor::assets {

/// Write `image` as a PNG into an `assets/` folder beside `documentPath` (or
/// beside `fallbackDir` when the document has not been saved yet) and return
/// the path to reference it by: relative to the document's directory when it
/// has one, otherwise absolute. Returns an empty string on any failure.
QString writePastedImage(const QImage& image, const QString& documentPath,
                         const QString& fallbackDir);

} // namespace hungryeditor::assets
