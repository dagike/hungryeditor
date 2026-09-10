#pragma once

#include <QString>

namespace hungryeditor::images {

/// Bytes above which a local image is left out of the preview rather than
/// inlined — a guard so one oversized file cannot stall rendering.
inline constexpr qint64 kDefaultMaxBytes = 10LL * 1024 * 1024;

/// Rewrite `<img>` tags in `html` so local image references render in the
/// preview offline: a readable image file within `maxBytes` becomes a
/// `data:` URI, resolved against `documentDir` when the path is relative.
///
/// Remote sources (`http:`, `https:`, `data:`, `qrc:`) are left untouched. A
/// path that does not resolve to a readable image loses its `src` and gains
/// `data-img-missing="<original>"`; one that resolves but is too large gains
/// `data-img-toobig="<original>"`. Either way the tag's `alt` text stays
/// visible. An empty `documentDir` means relative paths cannot be resolved.
QString inlineLocalImages(const QString& html, const QString& documentDir,
                          qint64 maxBytes = kDefaultMaxBytes);

} // namespace hungryeditor::images
