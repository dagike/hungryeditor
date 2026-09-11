#pragma once

#include <QString>

#include "theme/Theme.h"

namespace hungryeditor::themefile {

/// The outcome of loading a user theme file.
struct Result
{
    bool ok = false;   ///< false only for an unreadable file or unparsable JSON
    QString error;     ///< set when !ok
    Theme theme;       ///< Theme::builtin() with any recognised keys overridden
    QString customCss; ///< the file's "css" string, verbatim, or empty
};

/// Reads a JSON theme file: an object with any of the Theme field names
/// (background, text, muted, heading, link, codeText, codeBackground, border,
/// error) as hex-colour strings, each optional and defaulting to the light
/// theme's value when absent or invalid, plus an optional "css" string
/// appended after the generated preview CSS.
Result loadThemeFile(const QString& path);

} // namespace hungryeditor::themefile
