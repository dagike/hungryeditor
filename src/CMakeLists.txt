# The hungryeditor application.
#
# Application logic lives in the `hungryeditor_core` static library so that it
# can be exercised by the test tree; `hungryeditor` is a thin entry point.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

# Embed the tree-sitter highlight queries as C++ string literals.
include(EmbedQueries)
set(_md "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-markdown")
set(_gr "${CMAKE_SOURCE_DIR}/third_party/grammars")
hungryeditor_embed_queries("${CMAKE_CURRENT_BINARY_DIR}/generated/HighlightQueries.h"
    "kMarkdownHighlights|${_md}/tree-sitter-markdown/queries/highlights.scm"
    "kMarkdownInjections|${_md}/tree-sitter-markdown/queries/injections.scm"
    "kMarkdownInlineHighlights|${_md}/tree-sitter-markdown-inline/queries/highlights.scm"
    "kMarkdownInlineInjections|${_md}/tree-sitter-markdown-inline/queries/injections.scm"
    "kLangBash|${_gr}/bash/queries/highlights.scm"
    "kLangC|${_gr}/c/queries/highlights.scm"
    "kLangCpp|${_gr}/c/queries/highlights.scm|${_gr}/cpp/queries/highlights.scm"
    "kLangCSharp|${_gr}/c-sharp/queries/highlights.scm"
    "kLangCss|${_gr}/css/queries/highlights.scm"
    "kLangGo|${_gr}/go/queries/highlights.scm"
    "kLangHtml|${_gr}/html/queries/highlights.scm"
    "kLangJava|${_gr}/java/queries/highlights.scm"
    "kLangJavaScript|${_gr}/javascript/queries/highlights.scm"
    "kLangJson|${_gr}/json/queries/highlights.scm"
    "kLangPhp|${_gr}/php/queries/highlights.scm"
    "kLangPython|${_gr}/python/queries/highlights.scm"
    "kLangRuby|${_gr}/ruby/queries/highlights.scm"
    "kLangRust|${_gr}/rust/queries/highlights.scm"
    "kLangToml|${_gr}/toml/queries/highlights.scm"
    "kLangTypeScript|${_gr}/javascript/queries/highlights.scm|${_gr}/typescript/queries/highlights.scm"
    "kLangYaml|${_gr}/yaml/queries/highlights.scm")

add_library(hungryeditor_core STATIC
    app/CommandLine.cpp
    app/CommandLine.h
    app/MainWindow.cpp
    app/MainWindow.h
    app/TabBar.cpp
    app/TabBar.h
    editor/Document.cpp
    editor/Document.h
    editor/DocumentManager.cpp
    editor/DocumentManager.h
    editor/Editor.cpp
    editor/Editor.h
    highlight/CaptureStyles.cpp
    highlight/CaptureStyles.h
    highlight/GrammarRegistry.cpp
    highlight/GrammarRegistry.h
    highlight/TreeSitterEngine.cpp
    highlight/TreeSitterEngine.h
    highlight/HighlightWorker.cpp
    highlight/HighlightWorker.h
    highlight/HighlightController.cpp
    highlight/HighlightController.h
    io/DraftStore.cpp
    io/DraftStore.h
    io/RecentFiles.cpp
    io/RecentFiles.h
    io/SessionStore.cpp
    io/SessionStore.h
    io/TextFile.cpp
    io/TextFile.h
)
add_library(hungryeditor::core ALIAS hungryeditor_core)

set_target_properties(hungryeditor_core PROPERTIES AUTOMOC ON)

target_include_directories(hungryeditor_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_compile_definitions(hungryeditor_core PRIVATE
    HUNGRYEDITOR_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(hungryeditor_core PUBLIC
    Qt6::Widgets
    hungryeditor::scintilla
    hungryeditor::lexilla
    hungryeditor::treesitter
)

hungryeditor_set_warnings(hungryeditor_core)

# --- entry point ---------------------------------------------------------
add_executable(hungryeditor app/main.cpp)

set_target_properties(hungryeditor PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF)

target_compile_definitions(hungryeditor PRIVATE
    HUNGRYEDITOR_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(hungryeditor PRIVATE hungryeditor::core)

hungryeditor_set_warnings(hungryeditor)
