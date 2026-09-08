# Vendored dependencies, built as static libraries.
#
# These are third-party sources kept verbatim (only build/project files were
# stripped). They are compiled with warnings silenced and their headers are
# exposed as SYSTEM includes so they never pollute first-party diagnostics.
#
#   scintilla        5.5.5  (HPND-style, see scintilla/License.txt)
#   lexilla          5.4.0  (HPND-style, see lexilla/License.txt)
#   tree-sitter      0.24.7 (MIT, see tree-sitter/LICENSE)
#   tree-sitter-md   0.4.1  (MIT, see tree-sitter-markdown/LICENSE)

# --- helper: quiet a vendored target --------------------------------------
function(_vendor_quiet target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /w)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON)
endfunction()

# =========================================================================
# Scintilla  (editor engine + Qt platform layer)
# =========================================================================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)

set(_sci "${CMAKE_CURRENT_SOURCE_DIR}/scintilla")

file(GLOB _sci_core CONFIGURE_DEPENDS "${_sci}/src/*.cxx")
set(_sci_qt
    "${_sci}/qt/ScintillaEditBase/PlatQt.cpp"
    "${_sci}/qt/ScintillaEditBase/ScintillaQt.cpp"
    "${_sci}/qt/ScintillaEditBase/ScintillaEditBase.cpp")

# Scintilla::ScintillaCall — the type-safe C++ wrapper over the message API,
# used by src/editor/. Shipped separately from src/ upstream (in call/).
set(_sci_call "${_sci}/call/ScintillaCall.cxx")

add_library(scintilla STATIC ${_sci_core} ${_sci_qt} ${_sci_call})
add_library(hungryeditor::scintilla ALIAS scintilla)

set_target_properties(scintilla PROPERTIES AUTOMOC ON)

target_compile_definitions(scintilla PRIVATE SCINTILLA_QT=1 MAKING_LIBRARY=1)

# ScintillaEditBase.h pulls in headers from src/, so consumers of the Qt
# widget need src/ on their include path too — this mirrors the upstream
# .pro file (INCLUDEPATH += ../../include ../../src).
target_include_directories(scintilla SYSTEM PUBLIC
    "${_sci}/include"
    "${_sci}/src"
    "${_sci}/qt/ScintillaEditBase")

target_link_libraries(scintilla PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat)

_vendor_quiet(scintilla)

# =========================================================================
# Lexilla  (syntax lexers — used only for the large-file fallback path)
# =========================================================================
set(_lex "${CMAKE_CURRENT_SOURCE_DIR}/lexilla")

file(GLOB _lex_lib   CONFIGURE_DEPENDS "${_lex}/lexlib/*.cxx")
file(GLOB _lex_langs CONFIGURE_DEPENDS "${_lex}/lexers/*.cxx")

add_library(lexilla STATIC
    ${_lex_lib}
    ${_lex_langs}
    "${_lex}/src/Lexilla.cxx")
add_library(hungryeditor::lexilla ALIAS lexilla)

target_include_directories(lexilla
    PRIVATE
        "${_lex}/lexlib"
        "${_sci}/src"
    SYSTEM PUBLIC
        "${_lex}/include"
        "${_sci}/include")

_vendor_quiet(lexilla)

# =========================================================================
# tree-sitter  (incremental parser runtime + Markdown grammars, C11)
# =========================================================================
set(_ts   "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter")
set(_tsmd "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-markdown")

add_library(treesitter STATIC
    "${_ts}/src/lib.c"                                       # amalgamated runtime
    "${_tsmd}/tree-sitter-markdown/src/parser.c"
    "${_tsmd}/tree-sitter-markdown/src/scanner.c"
    "${_tsmd}/tree-sitter-markdown-inline/src/parser.c"
    "${_tsmd}/tree-sitter-markdown-inline/src/scanner.c")
add_library(hungryeditor::treesitter ALIAS treesitter)

# Each grammar's .c files resolve "tree_sitter/parser.h" from their own src/
# directory; only the runtime needs an explicit include path.
target_include_directories(treesitter
    PRIVATE "${_ts}/src"
    SYSTEM PUBLIC "${_ts}/include")

set_target_properties(treesitter PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(treesitter PRIVATE /w)
else()
    target_compile_options(treesitter PRIVATE -w)
endif()
