# Acquisition of the Microsoft WebView2 SDK for the planned (not yet built)
# WebView2 preview backend — see src/preview/PreviewBackendFactory.cpp,
# which always selects the QtWebEngine backend today regardless of this
# file. Included once from the top-level CMakeLists.txt, before
# add_subdirectory(src) so HUNGRYEDITOR_PREVIEW_BACKEND and the
# hungryeditor::webview2_sdk target it defines both exist by the time
# src/CMakeLists.txt looks for them.
#
# Deliberately breaks this repo's vendor-everything convention: the SDK is
# proprietary, its generated header (WebView2.h) is roughly 1.5 MB, and the
# nupkg ships prebuilt binaries alongside it — none of that belongs
# committed here. FetchContent at a pinned version and hash is the
# exception, same as third_party/'s CMake-level dependencies but without a
# vendored copy.
#
# Only the include directory is exposed. The eventual backend must not link
# WebView2Loader.lib: it resolves the two entry points it needs
# (CreateCoreWebView2EnvironmentWithOptions,
# GetAvailableCoreWebView2BrowserVersionString) via LoadLibraryW +
# GetProcAddress instead, so a missing Evergreen runtime on the target
# machine is an ordinary runtime `false` from a probe, not a load-time
# failure before main() ever runs.

set(HUNGRYEDITOR_PREVIEW_BACKEND "qtwebengine" CACHE STRING
    "Preview rendering backend to build against")
set_property(CACHE HUNGRYEDITOR_PREVIEW_BACKEND PROPERTY STRINGS "qtwebengine" "webview2")

if(HUNGRYEDITOR_PREVIEW_BACKEND STREQUAL "webview2")
    if(NOT WIN32)
        message(FATAL_ERROR "HUNGRYEDITOR_PREVIEW_BACKEND=webview2 is Windows-only.")
    endif()

    include(FetchContent)
    # Pinned to a specific stable release, not "latest" — an upstream
    # republish of the same package id must never silently change what a
    # from-scratch configure pulls in. Bump the version and hash together,
    # deliberately, verifying the new hash out of band.
    FetchContent_Declare(webview2
        URL "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4191.47/microsoft.web.webview2.1.0.4191.47.nupkg"
        URL_HASH SHA256=f492bbf547d0da329553b6727435b677579b1e9f91cc9e4a1ad029366d5f23d0)
    FetchContent_MakeAvailable(webview2)

    add_library(hungryeditor_webview2_sdk INTERFACE)
    target_include_directories(hungryeditor_webview2_sdk INTERFACE
        "${webview2_SOURCE_DIR}/build/native/include")
    add_library(hungryeditor::webview2_sdk ALIAS hungryeditor_webview2_sdk)
endif()
