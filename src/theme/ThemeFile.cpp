#include "theme/ThemeFile.h"

#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace hungryeditor::themefile {

namespace {

/// `field` in `root`, as a colour, when it parses as a valid QColor; `fallback`
/// otherwise (missing key, wrong type, or a string QColor doesn't recognise).
QColor colourOr(const QJsonObject& root, const QLatin1String& field, const QColor& fallback)
{
    const QJsonValue value = root.value(field);
    if (!value.isString()) {
        return fallback;
    }
    const QColor colour(value.toString());
    return colour.isValid() ? colour : fallback;
}

} // namespace

Result loadThemeFile(const QString& path)
{
    Result result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = parseError.error != QJsonParseError::NoError
                           ? parseError.errorString()
                           : QStringLiteral("Theme file must contain a JSON object");
        return result;
    }

    const QJsonObject root = doc.object();
    const Theme base = Theme::builtin();

    Theme theme;
    theme.background = colourOr(root, QLatin1String("background"), base.background);
    theme.text = colourOr(root, QLatin1String("text"), base.text);
    theme.muted = colourOr(root, QLatin1String("muted"), base.muted);
    theme.heading = colourOr(root, QLatin1String("heading"), base.heading);
    theme.link = colourOr(root, QLatin1String("link"), base.link);
    theme.codeText = colourOr(root, QLatin1String("codeText"), base.codeText);
    theme.codeBackground = colourOr(root, QLatin1String("codeBackground"), base.codeBackground);
    theme.border = colourOr(root, QLatin1String("border"), base.border);
    theme.error = colourOr(root, QLatin1String("error"), base.error);
    theme.keyword = colourOr(root, QLatin1String("keyword"), base.keyword);
    theme.type = colourOr(root, QLatin1String("type"), base.type);
    theme.function = colourOr(root, QLatin1String("function"), base.function);
    theme.string = colourOr(root, QLatin1String("string"), base.string);
    theme.comment = colourOr(root, QLatin1String("comment"), base.comment);
    theme.currentLine = colourOr(root, QLatin1String("currentLine"), base.currentLine);
    theme.selection = colourOr(root, QLatin1String("selection"), base.selection);
    theme.findMatch = colourOr(root, QLatin1String("findMatch"), base.findMatch);
    theme.braceMatch = colourOr(root, QLatin1String("braceMatch"), base.braceMatch);

    result.ok = true;
    result.theme = theme;
    result.customCss = root.value(QStringLiteral("css")).toString();
    return result;
}

} // namespace hungryeditor::themefile
