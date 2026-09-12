#pragma once

#include <memory>

class QObject;

namespace hungryeditor {

class PreviewBackend;

/// The preview rendering engines this build knows about. WebView2 exists as
/// a value today only so selectedPreviewEngine() has something to name once
/// the COM backend (a follow-on branch, not yet merged) lands — nothing
/// constructs one yet.
enum class PreviewEngine
{
    QtWebEngine,
    WebView2,
};

/// True if a WebView2 preview backend is both compiled into this binary and
/// its runtime is available on this machine. Always false today, on every
/// platform: the COM backend that would back it is a follow-on branch,
/// gated on a human running it on real Windows before merge, so there is
/// nothing yet for a `true` here to describe. Once it lands, the Windows
/// implementation probes the Evergreen client registry key (the same one
/// .github/workflows/ci.yml's WebView2 probe reads); every other platform
/// stays false permanently, since only Windows can ever host it.
bool isWebView2Available();

/// The engine ensurePreviewCreated() should build. Honors the
/// HUNGRYEDITOR_PREVIEW_ENGINE environment variable ("qtwebengine" or
/// "webview2") when set to a name that is actually available, which is
/// invaluable when one engine is broken on a particular machine; any other
/// value (unset, a typo, or "webview2" without a usable runtime) falls back
/// to QtWebEngine rather than failing to start the preview at all.
PreviewEngine selectedPreviewEngine();

/// Construct the backend selectedPreviewEngine() names, parented to
/// `parent`. Always succeeds: selectedPreviewEngine() never names an engine
/// this cannot actually build.
std::unique_ptr<PreviewBackend> createPreviewBackend(QObject* parent = nullptr);

} // namespace hungryeditor
