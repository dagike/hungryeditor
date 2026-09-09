#include "markdown/Md4cRenderer.h"

#include <algorithm>
#include <vector>

#include <QByteArray>

#include <md4c.h>

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
};

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
    case MD_BLOCK_LI:
        openBlock(ctx, "<li", ">");
        break;
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
        if (d->lang.text != nullptr && d->lang.size > 0) {
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
    default:
        break; // table blocks arrive with GFM, added later
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
        closeBlock(ctx, "</code></pre>\n");
        break;
    case MD_BLOCK_HTML:
        break;
    case MD_BLOCK_P:
        closeBlock(ctx, "</p>\n");
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

} // namespace

QString Md4cRenderer::toHtml(const QString& markdown) const
{
    const QByteArray input = markdown.toUtf8();

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
    parser.flags = 0; // CommonMark
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

    return QString::fromUtf8(ctx.out);
}

} // namespace hungryeditor
