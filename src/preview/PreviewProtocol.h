#pragma once

#include <QString>

namespace hungryeditor::preview_protocol {

/// The `type` field of a page -> host wire message. Every message on the
/// channel is a JSON object shaped `{"type": "...", ...payload}`; this is
/// the one field every backend (QWebChannel today, WebView2 eventually)
/// needs to dispatch on.
enum class PageMessageType
{
    /// The page's transport is up and it wants the current theme and
    /// content pushed down. Sent once, first — there are no channel
    /// properties to read instead, since WebView2 has no equivalent.
    Hello,
    /// The page finished applying the content it was just sent.
    Ready,
    /// The viewer scrolled; `line` is the source line now at the top.
    Scroll,
    /// The viewer clicked a heading; `line` is its source line.
    HeadingClick,
    /// The viewer toggled a task-list checkbox; `line` is the source line of
    /// its list item and `checked` is the checkbox's new state.
    TaskToggle,
    /// Anything else: malformed JSON, a missing/wrong-typed field, or a
    /// `type` this host doesn't recognise. Never fails — callers should
    /// just ignore it.
    Unknown,
};

/// A decoded page -> host message. `line` and `checked` are populated only
/// for the message types that carry them; otherwise left at their default.
struct PageMessage
{
    PageMessageType type = PageMessageType::Unknown;
    int line = 0;
    bool checked = false;
};

/// Parse one wire message sent up from the page.
PageMessage decodePageMessage(const QString& json);

/// The host -> page messages, encoded to wire JSON.
QString encodeTheme(const QString& css);
QString encodeContent(const QString& html);
QString encodeScrollToLine(int line);

} // namespace hungryeditor::preview_protocol
