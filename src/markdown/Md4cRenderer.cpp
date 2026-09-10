#include "markdown/Md4cRenderer.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <md4c.h>

#include "highlight/CaptureStyles.h"
#include "highlight/CodeHighlighter.h"

namespace hungryeditor {

namespace {

/// A spot in the output where a block's `data-src-line` digits get inserted
/// once the source line is known. `line` stays -1 until resolved; `bias` shifts
/// the resolved value (a fenced code block's content starts one line below the
/// opening fence, which is the anchor we want).
struct LineHole
{
    qsizetype pos;
    int line = -1;
    int bias = 0;
};

struct RenderContext
{
    QByteArray out;
    const char* base = nullptr;
    MD_SIZE size = 0;

    std::vector<qsizetype> lineStarts;  ///< byte offset of each source line
    std::vector<LineHole> holes;        ///< one per block, in document order
    std::vector<std::size_t> openHoles; ///< holes for blocks not yet closed
    int lastLine = 0;                   ///< line of the most recent source text

    int imageDepth = 0; ///< inside <img>: text is folded into the alt attribute

    bool inCode = false; ///< inside a fenced/indented code block
    QByteArray codeText; ///< its verbatim content, held back for highlighting
    QByteArray codeLang; ///< its info string
};

/// The `style="text-align:…"` value for a GFM table column, or nullptr when the
/// column has no explicit alignment.
const char* alignStyle(MD_ALIGN align)
{
    switch (align) {
    case MD_ALIGN_LEFT:
        return "left";
    case MD_ALIGN_CENTER:
        return "center";
    case MD_ALIGN_RIGHT:
        return "right";
    case MD_ALIGN_DEFAULT:
    default:
        return nullptr;
    }
}

void appendEscaped(QByteArray& out, const char* text, MD_SIZE size)
{
    out.reserve(out.size() + qsizetype(size));
    for (MD_SIZE i = 0; i < size; ++i) {
        switch (text[i]) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += text[i];
            break; // raw byte keeps UTF-8 intact
        }
    }
}

void appendAttribute(QByteArray& out, const MD_ATTRIBUTE& attr)
{
    if (attr.text == nullptr) {
        return;
    }
    // md4c may split an attribute into typed substrings; entities are already
    // in their `&name;` form and pass through, everything else is escaped.
    for (unsigned i = 0; attr.substr_offsets[i] < attr.size; ++i) {
        const char* piece = attr.text + attr.substr_offsets[i];
        const MD_SIZE len = attr.substr_offsets[i + 1] - attr.substr_offsets[i];
        if (attr.substr_types[i] == MD_TEXT_ENTITY) {
            out.append(piece, qsizetype(len));
        } else {
            appendEscaped(out, piece, len);
        }
    }
}

int offsetToLine(const RenderContext& ctx, qsizetype offset)
{
    const auto it = std::upper_bound(ctx.lineStarts.begin(), ctx.lineStarts.end(), offset);
    return int(it - ctx.lineStarts.begin()) - 1;
}

/// Emit `<tag ...>` with an empty `data-src-line` slot and register the hole.
void openBlock(RenderContext& ctx, const char* openingBeforeSlot, const char* openingAfterSlot,
               int bias = 0)
{
    ctx.out += openingBeforeSlot;
    ctx.out += " data-src-line=\"";
    const qsizetype pos = ctx.out.size();
    ctx.out += '"';
    ctx.out += openingAfterSlot;
    ctx.holes.push_back({pos, -1, bias});
    ctx.openHoles.push_back(ctx.holes.size() - 1);
}

void closeBlock(RenderContext& ctx, const char* closing)
{
    if (!ctx.openHoles.empty()) {
        LineHole& hole = ctx.holes[ctx.openHoles.back()];
        ctx.openHoles.pop_back();
        if (hole.line < 0) {
            hole.line = std::max(0, ctx.lastLine + hole.bias);
        }
    }
    ctx.out += closing;
}

/// Flush a fenced block's buffered text: tree-sitter tokens wrapped in
/// `<span class="tok-...">`, or plain escaped text when the language is
/// unknown.
void emitHighlightedCode(RenderContext& ctx)
{
    const std::string_view code(ctx.codeText.constData(), std::size_t(ctx.codeText.size()));
    const std::string_view lang(ctx.codeLang.constData(), std::size_t(ctx.codeLang.size()));

    const std::vector<CodeToken> tokens = highlightCode(lang, code);
    if (tokens.empty()) {
        appendEscaped(ctx.out, code.data(), MD_SIZE(code.size()));
        return;
    }

    for (const CodeToken& token : tokens) {
        const char* piece = code.data() + token.start;
        const std::string cssClass = styleCssClass(token.style);
        if (cssClass.empty()) {
            appendEscaped(ctx.out, piece, MD_SIZE(token.length));
            continue;
        }
        ctx.out += "<span class=\"";
        ctx.out += cssClass.c_str();
        ctx.out += "\">";
        appendEscaped(ctx.out, piece, MD_SIZE(token.length));
        ctx.out += "</span>";
    }
}

int enterBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    auto& ctx = *static_cast<RenderContext*>(userdata);
    switch (type) {
    case MD_BLOCK_DOC:
        break;
    case MD_BLOCK_QUOTE:
        openBlock(ctx, "<blockquote", ">");
        break;
    case MD_BLOCK_UL:
        openBlock(ctx, "<ul", ">");
        break;
    case MD_BLOCK_OL: {
        const auto* d = static_cast<const MD_BLOCK_OL_DETAIL*>(detail);
        if (d->start != 1) {
            const QByteArray after = " start=\"" + QByteArray::number(int(d->start)) + "\">";
            openBlock(ctx, "<ol", after.constData());
        } else {
            openBlock(ctx, "<ol", ">");
        }
        break;
    }
    case MD_BLOCK_LI: {
        const auto* d = static_cast<const MD_BLOCK_LI_DETAIL*>(detail);
        if (d != nullptr && d->is_task != 0) {
            openBlock(ctx, "<li class=\"task-list-item\"", ">");
            ctx.out += "<input type=\"checkbox\" disabled";
            if (d->task_mark == 'x' || d->task_mark == 'X') {
                ctx.out += " checked";
            }
            ctx.out += "> ";
        } else {
            openBlock(ctx, "<li", ">");
        }
        break;
    }
    case MD_BLOCK_HR:
        openBlock(ctx, "<hr", ">");
        break;
    case MD_BLOCK_H: {
        const unsigned level = static_cast<const MD_BLOCK_H_DETAIL*>(detail)->level;
        const QByteArray open = "<h" + QByteArray::number(int(level));
        openBlock(ctx, open.constData(), ">");
        break;
    }
    case MD_BLOCK_CODE: {
        const auto* d = static_cast<const MD_BLOCK_CODE_DETAIL*>(detail);
        // A fence line precedes the content md4c reports the offset of.
        openBlock(ctx, "<pre", "><code", d->fence_char != 0 ? -1 : 0);
        ctx.inCode = true;
        ctx.codeText.clear();
        ctx.codeLang.clear();
        if (d->lang.text != nullptr && d->lang.size > 0) {
            ctx.codeLang = QByteArray(d->lang.text, qsizetype(d->lang.size));
            ctx.out += " class=\"language-";
            appendAttribute(ctx.out, d->lang);
            ctx.out += '"';
        }
        ctx.out += '>';
        break;
    }
    case MD_BLOCK_HTML:
        break; // raw: emitted verbatim by the text callback
    case MD_BLOCK_P:
        openBlock(ctx, "<p", ">");
        break;
    case MD_BLOCK_TABLE:
        openBlock(ctx, "<table", ">");
        break;
    case MD_BLOCK_THEAD:
        ctx.out += "<thead>\n";
        break;
    case MD_BLOCK_TBODY:
        ctx.out += "<tbody>\n";
        break;
    case MD_BLOCK_TR:
        openBlock(ctx, "<tr", ">");
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
        const auto* d = static_cast<const MD_BLOCK_TD_DETAIL*>(detail);
        ctx.out += (type == MD_BLOCK_TH) ? "<th" : "<td";
        const char* style = alignStyle(d != nullptr ? d->align : MD_ALIGN_DEFAULT);
        if (style != nullptr) {
            ctx.out += " style=\"text-align:";
            ctx.out += style;
            ctx.out += '"';
        }
        ctx.out += '>';
        break;
    }
    default:
        break;
    }
    return 0;
}

int leaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    auto& ctx = *static_cast<RenderContext*>(userdata);
    switch (type) {
    case MD_BLOCK_DOC:
        break;
    case MD_BLOCK_QUOTE:
        closeBlock(ctx, "</blockquote>\n");
        break;
    case MD_BLOCK_UL:
        closeBlock(ctx, "</ul>\n");
        break;
    case MD_BLOCK_OL:
        closeBlock(ctx, "</ol>\n");
        break;
    case MD_BLOCK_LI:
        closeBlock(ctx, "</li>\n");
        break;
    case MD_BLOCK_HR:
        closeBlock(ctx, "\n");
        break;
    case MD_BLOCK_H: {
        const unsigned level = static_cast<const MD_BLOCK_H_DETAIL*>(detail)->level;
        const QByteArray close = "</h" + QByteArray::number(int(level)) + ">\n";
        closeBlock(ctx, close.constData());
        break;
    }
    case MD_BLOCK_CODE:
        ctx.inCode = false;
        emitHighlightedCode(ctx);
        closeBlock(ctx, "</code></pre>\n");
        break;
    case MD_BLOCK_HTML:
        break;
    case MD_BLOCK_P:
        closeBlock(ctx, "</p>\n");
        break;
    case MD_BLOCK_TABLE:
        closeBlock(ctx, "</table>\n");
        break;
    case MD_BLOCK_THEAD:
        ctx.out += "</thead>\n";
        break;
    case MD_BLOCK_TBODY:
        ctx.out += "</tbody>\n";
        break;
    case MD_BLOCK_TR:
        closeBlock(ctx, "</tr>\n");
        break;
    case MD_BLOCK_TH:
        ctx.out += "</th>";
        break;
    case MD_BLOCK_TD:
        ctx.out += "</td>";
        break;
    default:
        break;
    }
    return 0;
}

int enterSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    auto& ctx = *static_cast<RenderContext*>(userdata);
    switch (type) {
    case MD_SPAN_EM:
        ctx.out += "<em>";
        break;
    case MD_SPAN_STRONG:
        ctx.out += "<strong>";
        break;
    case MD_SPAN_DEL:
        ctx.out += "<del>";
        break;
    case MD_SPAN_CODE:
        ctx.out += "<code>";
        break;
    case MD_SPAN_A: {
        const auto* d = static_cast<const MD_SPAN_A_DETAIL*>(detail);
        ctx.out += "<a href=\"";
        appendAttribute(ctx.out, d->href);
        ctx.out += '"';
        if (d->title.text != nullptr && d->title.size > 0) {
            ctx.out += " title=\"";
            appendAttribute(ctx.out, d->title);
            ctx.out += '"';
        }
        ctx.out += '>';
        break;
    }
    case MD_SPAN_IMG: {
        const auto* d = static_cast<const MD_SPAN_IMG_DETAIL*>(detail);
        ctx.out += "<img src=\"";
        appendAttribute(ctx.out, d->src);
        ctx.out += "\" alt=\"";
        ++ctx.imageDepth;
        break;
    }
    default:
        break;
    }
    return 0;
}

int leaveSpan(MD_SPANTYPE type, void* detail, void* userdata)
{
    auto& ctx = *static_cast<RenderContext*>(userdata);
    switch (type) {
    case MD_SPAN_EM:
        ctx.out += "</em>";
        break;
    case MD_SPAN_STRONG:
        ctx.out += "</strong>";
        break;
    case MD_SPAN_DEL:
        ctx.out += "</del>";
        break;
    case MD_SPAN_CODE:
        ctx.out += "</code>";
        break;
    case MD_SPAN_A:
        ctx.out += "</a>";
        break;
    case MD_SPAN_IMG: {
        const auto* d = static_cast<const MD_SPAN_IMG_DETAIL*>(detail);
        --ctx.imageDepth;
        if (d->title.text != nullptr && d->title.size > 0) {
            ctx.out += "\" title=\"";
            appendAttribute(ctx.out, d->title);
        }
        ctx.out += "\">";
        break;
    }
    default:
        break;
    }
    return 0;
}

int onText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    auto& ctx = *static_cast<RenderContext*>(userdata);

    // Text runs that are slices of the source let us pin the open blocks to a
    // line. Synthetic runs (hard/soft breaks) point at a literal, not the input.
    if (text >= ctx.base && text < ctx.base + ctx.size) {
        const int line = offsetToLine(ctx, text - ctx.base);
        ctx.lastLine = line;
        for (const std::size_t hi : ctx.openHoles) {
            if (ctx.holes[hi].line < 0) {
                ctx.holes[hi].line = std::max(0, line + ctx.holes[hi].bias);
            }
        }
    }

    if (ctx.imageDepth > 0) {
        if (type == MD_TEXT_BR || type == MD_TEXT_SOFTBR) {
            ctx.out += ' ';
        } else if (type == MD_TEXT_ENTITY) {
            ctx.out.append(text, qsizetype(size));
        } else {
            appendEscaped(ctx.out, text, size);
        }
        return 0;
    }

    if (ctx.inCode) {
        ctx.codeText.append(text, qsizetype(size)); // highlighted on leave_block
        return 0;
    }

    switch (type) {
    case MD_TEXT_NULLCHAR:
        ctx.out += "\xEF\xBF\xBD";
        break; // U+FFFD
    case MD_TEXT_BR:
        ctx.out += "<br>\n";
        break;
    case MD_TEXT_SOFTBR:
        ctx.out += '\n';
        break;
    case MD_TEXT_HTML:
    case MD_TEXT_ENTITY:
        ctx.out.append(text, qsizetype(size));
        break; // verbatim
    default:
        appendEscaped(ctx.out, text, size);
        break;
    }
    return 0;
}

/// The result of lifting footnotes out of the source: definitions removed (the
/// lines blanked so every other block keeps its `data-src-line`), inline
/// `[^id]` references rewritten into the HTML md4c passes straight through.
struct FootnoteData
{
    QString markdown;                    ///< rewritten source
    QStringList orderedIds;              ///< referenced ids, first-reference order
    QHash<QString, QString> definitions; ///< id -> raw definition text
};

FootnoteData extractFootnotes(const QString& source)
{
    static const QRegularExpression defPattern(
        QStringLiteral("^\\[\\^([^\\]\\s]+)\\]:[ \\t]*(.*)$"));
    static const QRegularExpression refPattern(QStringLiteral("\\[\\^([^\\]\\s]+)\\]"));
    static const QRegularExpression fencePattern(QStringLiteral("^ {0,3}(`{3,}|~{3,})"));

    QStringList lines = source.split(QLatin1Char('\n'));
    FootnoteData data;

    // Pass 1 - pull single-line definitions out, blanking their lines.
    bool inFence = false;
    for (QString& line : lines) {
        if (fencePattern.match(line).hasMatch()) {
            inFence = !inFence;
            continue;
        }
        if (inFence) {
            continue;
        }
        const QRegularExpressionMatch m = defPattern.match(line);
        if (m.hasMatch()) {
            data.definitions.insert(m.captured(1), m.captured(2).trimmed());
            line.clear();
        }
    }

    if (data.definitions.isEmpty()) {
        data.markdown = source;
        return data;
    }

    // Pass 2 - rewrite `[^id]` references that resolve to a definition. A
    // reference with no matching definition is left as literal text.
    QSet<QString> anchored;
    inFence = false;
    for (QString& line : lines) {
        if (fencePattern.match(line).hasMatch()) {
            inFence = !inFence;
            continue;
        }
        if (inFence || !line.contains(QStringLiteral("[^"))) {
            continue;
        }
        QString rebuilt;
        qsizetype cursor = 0;
        bool changed = false;
        QRegularExpressionMatchIterator it = refPattern.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QString id = m.captured(1);
            if (!data.definitions.contains(id)) {
                continue;
            }
            if (!data.orderedIds.contains(id)) {
                data.orderedIds.append(id);
            }
            rebuilt += line.mid(cursor, m.capturedStart() - cursor);
            cursor = m.capturedEnd();
            changed = true;

            const int number = static_cast<int>(data.orderedIds.indexOf(id)) + 1;
            rebuilt += QStringLiteral("<sup class=\"fn-ref\"><a href=\"#fn-%1\"").arg(id);
            if (!anchored.contains(id)) {
                rebuilt += QStringLiteral(" id=\"fnref-%1\"").arg(id);
                anchored.insert(id);
            }
            rebuilt += QStringLiteral(">%1</a></sup>").arg(number);
        }
        if (changed) {
            rebuilt += line.mid(cursor);
            line = rebuilt;
        }
    }

    data.markdown = lines.join(QLatin1Char('\n'));
    return data;
}

/// Drop a single enclosing `<p>` so a footnote body renders inline in its `<li>`.
QString stripParagraphWrapper(QString html)
{
    static const QRegularExpression open(QStringLiteral("\\A<p\\b[^>]*>"));
    static const QRegularExpression close(QStringLiteral("</p>\\s*\\z"));
    html = html.trimmed();
    html.remove(open);
    html.remove(close);
    return html.trimmed();
}

} // namespace

QString Md4cRenderer::toHtml(const QString& markdown) const
{
    const FootnoteData footnotes = extractFootnotes(markdown);
    const QByteArray input = footnotes.markdown.toUtf8();

    RenderContext ctx;
    ctx.base = input.constData();
    ctx.size = MD_SIZE(input.size());
    ctx.lineStarts.push_back(0);
    for (qsizetype i = 0; i < input.size(); ++i) {
        if (input[i] == '\n') {
            ctx.lineStarts.push_back(i + 1);
        }
    }

    MD_PARSER parser{};
    parser.abi_version = 0;
    parser.flags = MD_DIALECT_GITHUB; // tables, task lists, strikethrough, autolinks
    parser.enter_block = enterBlock;
    parser.leave_block = leaveBlock;
    parser.enter_span = enterSpan;
    parser.leave_span = leaveSpan;
    parser.text = onText;

    md_parse(input.constData(), ctx.size, &parser, &ctx);

    // Fill the data-src-line slots. Holes are in ascending output position, so
    // inserting back-to-front leaves the earlier positions valid.
    for (auto it = ctx.holes.rbegin(); it != ctx.holes.rend(); ++it) {
        ctx.out.insert(it->pos, QByteArray::number(std::max(it->line, 0)));
    }

    QString html = QString::fromUtf8(ctx.out);

    if (!footnotes.orderedIds.isEmpty()) {
        const int srcLine = static_cast<int>(ctx.lineStarts.size()) - 1;
        html += QStringLiteral("<section class=\"footnotes\" data-src-line=\"%1\">\n<ol>\n")
                    .arg(srcLine);
        for (const QString& id : footnotes.orderedIds) {
            const QString body = stripParagraphWrapper(toHtml(footnotes.definitions.value(id)));
            html += QStringLiteral("<li id=\"fn-%1\">").arg(id);
            html += body;
            html += QStringLiteral(" <a href=\"#fnref-%1\" class=\"fn-backref\">&#8617;</a></li>\n")
                        .arg(id);
        }
        html += QStringLiteral("</ol>\n</section>\n");
    }

    return html;
}

} // namespace hungryeditor
