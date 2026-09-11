#include "theme/Theme.h"

#include "highlight/CaptureStyles.h"

namespace hungryeditor {

namespace {

Theme make(const char* background, const char* text, const char* muted, const char* heading,
           const char* link, const char* codeText, const char* codeBackground, const char* border,
           const char* error)
{
    Theme t;
    t.background = QColor(QString::fromLatin1(background));
    t.text = QColor(QString::fromLatin1(text));
    t.muted = QColor(QString::fromLatin1(muted));
    t.heading = QColor(QString::fromLatin1(heading));
    t.link = QColor(QString::fromLatin1(link));
    t.codeText = QColor(QString::fromLatin1(codeText));
    t.codeBackground = QColor(QString::fromLatin1(codeBackground));
    t.border = QColor(QString::fromLatin1(border));
    t.error = QColor(QString::fromLatin1(error));
    return t;
}

} // namespace

Theme Theme::builtin()
{
    return forBuiltin(Builtin::Light);
}

Theme Theme::forBuiltin(Builtin id)
{
    switch (id) {
    case Builtin::Dark:
        return make("#1e1e1e", "#d4d4d4", "#9aa0a6", "#4fc1ff", "#3794ff", "#ce9178", "#2d2d2d",
                    "#3c3c3c", "#f14c4c");
    case Builtin::HighContrast:
        return make("#000000", "#ffffff", "#d0d0d0", "#ffff00", "#ffff00", "#00ffff", "#1a1a1a",
                    "#ffffff", "#ff6060");
    case Builtin::Sepia:
        return make("#f4ecd8", "#5b4636", "#8a7860", "#7a4a2b", "#956a3c", "#7a4a2b", "#ece0c6",
                    "#d8c9a3", "#b5432f");
    case Builtin::Light:
    default:
        return make("#ffffff", "#1e1e1e", "#57606a", "#0550ae", "#0969da", "#6e40c9", "#f6f8fa",
                    "#d0d7de", "#cf222e");
    }
}

QString Theme::builtinName(Builtin id)
{
    switch (id) {
    case Builtin::Dark:
        return QStringLiteral("Dark");
    case Builtin::HighContrast:
        return QStringLiteral("High Contrast");
    case Builtin::Sepia:
        return QStringLiteral("Sepia");
    case Builtin::Light:
    default:
        return QStringLiteral("Light");
    }
}

QString Theme::builtinKey(Builtin id)
{
    switch (id) {
    case Builtin::Dark:
        return QStringLiteral("dark");
    case Builtin::HighContrast:
        return QStringLiteral("high-contrast");
    case Builtin::Sepia:
        return QStringLiteral("sepia");
    case Builtin::Light:
    default:
        return QStringLiteral("light");
    }
}

Theme::Builtin Theme::builtinFromKey(const QString& key, Builtin fallback)
{
    if (key == QLatin1String("dark")) {
        return Builtin::Dark;
    }
    if (key == QLatin1String("high-contrast")) {
        return Builtin::HighContrast;
    }
    if (key == QLatin1String("sepia")) {
        return Builtin::Sepia;
    }
    if (key == QLatin1String("light")) {
        return Builtin::Light;
    }
    return fallback;
}

QString Theme::previewCss() const
{
    const QString bg = background.name();
    const QString fg = text.name();
    const QString mut = muted.name();
    const QString head = heading.name();
    const QString lnk = link.name();
    const QString codeFg = codeText.name();
    const QString codeBg = codeBackground.name();
    const QString bord = border.name();
    const QString err = error.name();

    return QStringLiteral(
               ":root {"
               "  --he-bg: %1;"
               "  --he-fg: %2;"
               "  --he-muted: %3;"
               "  --he-heading: %4;"
               "  --he-link: %5;"
               "  --he-code-fg: %6;"
               "  --he-code-bg: %7;"
               "  --he-border: %8;"
               "  --he-error: %9;"
               "}"
               "body { background: var(--he-bg); color: var(--he-fg); }"
               "a { color: var(--he-link); }"
               "h1, h2, h3, h4, h5, h6 { color: var(--he-heading); line-height: 1.25; }"
               "h1 { border-bottom: 1px solid var(--he-border); padding-bottom: .3em; }"
               "code {"
               "  background: var(--he-code-bg); color: var(--he-code-fg);"
               "  padding: .15em .35em; border-radius: 4px;"
               "  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;"
               "  font-size: .9em;"
               "}"
               "pre {"
               "  background: var(--he-code-bg); border: 1px solid var(--he-border);"
               "  border-radius: 6px; padding: .85em 1em; overflow-x: auto;"
               "}"
               "pre code { background: none; padding: 0; font-size: .875em; }"
               "blockquote {"
               "  margin: 0; padding: 0 1em; color: var(--he-muted);"
               "  border-left: .25em solid var(--he-border);"
               "}"
               "hr { border: 0; border-top: 1px solid var(--he-border); margin: 1.5em 0; }"
               "table { border-collapse: collapse; }"
               "th, td { border: 1px solid var(--he-border); padding: .4em .75em; }"
               "thead th { background: var(--he-code-bg); }"
               "li.task-list-item { list-style: none; }"
               "li.task-list-item > input { margin: 0 .45em 0 -1.35em; cursor: pointer; }"
               ".footnotes {"
               "  margin-top: 2em; padding-top: 1em;"
               "  border-top: 1px solid var(--he-border);"
               "  font-size: .875em; color: var(--he-muted);"
               "}"
               "sup.fn-ref a, .footnotes a.fn-backref { text-decoration: none; }"
               ".front-matter-card {"
               "  margin: 0 0 1.5em; padding: .6em 1em;"
               "  border: 1px solid var(--he-border); border-radius: 6px;"
               "  background: var(--he-code-bg); font-size: .9em;"
               "}"
               ".front-matter-card dl {"
               "  margin: 0; display: grid;"
               "  grid-template-columns: auto 1fr; gap: .15em .8em;"
               "}"
               ".front-matter-card dt { color: var(--he-muted); font-weight: 600; }"
               ".front-matter-card dd { margin: 0; }"
               ".mermaid-diagram { margin: 1em 0; text-align: center; }"
               ".mermaid-diagram svg { max-width: 100%; height: auto; }"
               ".mermaid-diagram.mermaid-pending { min-height: 3em; }"
               ".math-display { display: block; overflow-x: auto; margin: 1em 0; }"
               "img { max-width: 100%; }"
               "img[data-img-missing], img[data-img-toobig] {"
               "  display: inline-block; min-width: 8em; min-height: 3em; padding: .4em .6em;"
               "  border: 1px dashed var(--he-border); border-radius: 6px;"
               "  color: var(--he-muted); font-size: .85em; font-style: italic;"
               "}"
               ".he-render-error {"
               "  margin: 1em 0; padding: .6em .8em; border-radius: 6px;"
               "  border: 1px solid var(--he-error); background: var(--he-code-bg);"
               "  color: var(--he-error); font-size: .9em;"
               "}"
               ".he-render-error strong { display: block; margin-bottom: .3em; }"
               ".he-render-error pre {"
               "  margin: 0; padding: 0; border: 0; background: none;"
               "  color: inherit; font-size: .95em; white-space: pre-wrap;"
               "}"
               ".he-render-error-inline {"
               "  color: var(--he-error); border-bottom: 1px dotted var(--he-error);"
               "  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;"
               "}")
               .arg(bg, fg, mut, head, lnk, codeFg, codeBg, bord, err) +
           codeTokenCss();
}

QString Theme::codeTokenCss() const
{
    // Fenced-code colouring: the same token palette the editor paints with, so
    // a `rust fence looks identical in both panes.
    QString css;
    for (const StyleDef& style : styleTable()) {
        const std::string cssClass = styleCssClass(style.id);
        if (cssClass.empty()) {
            continue;
        }
        css += QStringLiteral(".%1 { color: %2;")
                   .arg(QString::fromStdString(cssClass), style.foreground.name());
        if (style.bold) {
            css += QStringLiteral(" font-weight: 600;");
        }
        if (style.italic) {
            css += QStringLiteral(" font-style: italic;");
        }
        if (style.underline) {
            css += QStringLiteral(" text-decoration: underline;");
        }
        css += QStringLiteral(" }");
    }
    return css;
}

} // namespace hungryeditor
