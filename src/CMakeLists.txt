# The hungryeditor application.
#
# Application logic lives in the `hungryeditor_core` static library so that it
# can be exercised by the test tree; `hungryeditor` is a thin entry point.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

# Embed the tree-sitter highlight queries as C++ string literals.
include(EmbedQueries)
set(_md "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-markdown")
hungryeditor_embed_queries("${CMAKE_CURRENT_BINARY_DIR}/generated/HighlightQueries.h"
    kMarkdownHighlights       "${_md}/tree-sitter-markdown/queries/highlights.scm"
    kMarkdownInlineHighlights "${_md}/tree-sitter-markdown-inline/queries/highlights.scm"
    kMarkdownInlineInjections "${_md}/tree-sitter-markdown-inline/queries/injections.scm")

add_library(hungryeditor_core STATIC
    app/MainWindow.cpp
    app/MainWindow.h
    editor/Editor.cpp
    editor/Editor.h
    highlight/CaptureStyles.cpp
    highlight/CaptureStyles.h
    highlight/TreeSitterEngine.cpp
    highlight/TreeSitterEngine.h
    highlight/HighlightWorker.cpp
    highlight/HighlightWorker.h
    highlight/HighlightController.cpp
    highlight/HighlightController.h
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
