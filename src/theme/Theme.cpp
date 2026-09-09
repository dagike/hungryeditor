#include "theme/Theme.h"

namespace hungryeditor {

Theme Theme::builtin()
{
    Theme t;
    t.background = QColor(QStringLiteral("#ffffff"));
    t.text = QColor(QStringLiteral("#1e1e1e"));
    t.muted = QColor(QStringLiteral("#57606a"));
    t.heading = QColor(QStringLiteral("#0550ae"));
    t.link = QColor(QStringLiteral("#0969da"));
    t.codeText = QColor(QStringLiteral("#6e40c9"));
    t.codeBackground = QColor(QStringLiteral("#f6f8fa"));
    t.border = QColor(QStringLiteral("#d0d7de"));
    return t;
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
               "img { max-width: 100%; }")
        .arg(bg, fg, mut, head, lnk, codeFg, codeBg, bord);
}

} // namespace hungryeditor
