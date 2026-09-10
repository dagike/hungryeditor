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
#   grammars/*       18 language grammars (all MIT, see THIRD_PARTY.md)
#   md4c             0.5.2  (MIT, see md4c/LICENSE.md)
#   mermaid          10.9.1 (MIT, see mermaid/LICENSE) — data file, embedded as a
#                           Qt resource for the preview; no build target here

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
set(_gr   "${CMAKE_CURRENT_SOURCE_DIR}/grammars")

# Language grammars with a flat src/ layout.
set(_gr_langs
    bash c cpp c-sharp css go html java javascript json
    python ruby rust toml yaml)
set(_gr_sources "")
foreach(_lang IN LISTS _gr_langs)
    file(GLOB _lang_c CONFIGURE_DEPENDS "${_gr}/${_lang}/src/*.c")
    list(APPEND _gr_sources ${_lang_c})
endforeach()

# TypeScript/TSX and PHP keep one directory level so each dialect's scanner
# can reach its package's shared common/ header.
file(GLOB _ts_dialect_c CONFIGURE_DEPENDS
    "${_gr}/typescript/typescript/src/*.c"
    "${_gr}/typescript/tsx/src/*.c"
    "${_gr}/php/php/src/*.c")
list(APPEND _gr_sources ${_ts_dialect_c})

# yaml/src/scanner.c #includes schema.core.c directly — never compile the
# schema fragments as their own translation units.
list(FILTER _gr_sources EXCLUDE REGEX "/schema\\.[a-z]+\\.c$")

add_library(treesitter STATIC
    "${_ts}/src/lib.c"                                       # amalgamated runtime
    "${_tsmd}/tree-sitter-markdown/src/parser.c"
    "${_tsmd}/tree-sitter-markdown/src/scanner.c"
    "${_tsmd}/tree-sitter-markdown-inline/src/parser.c"
    "${_tsmd}/tree-sitter-markdown-inline/src/scanner.c"
    ${_gr_sources})
add_library(hungryeditor::treesitter ALIAS treesitter)

# Most grammar .c files resolve "tree_sitter/parser.h" from their own src/
# directory. The TS/TSX/PHP scanners include a sibling common/scanner.h, which
# in turn needs the dialect's src/ dir on the header search path.
set_source_files_properties(
    "${_gr}/typescript/typescript/src/scanner.c"
    "${_gr}/typescript/tsx/src/scanner.c"
    "${_gr}/php/php/src/scanner.c"
    PROPERTIES INCLUDE_DIRECTORIES
        "${_gr}/typescript/typescript/src;${_gr}/typescript/tsx/src;${_gr}/php/php/src")

# The runtime needs its own src/ on the include path; grammars do not.
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

# =========================================================================
# md4c  (CommonMark + GFM parser — feeds the HTML preview, C99)
# =========================================================================
set(_md4c "${CMAKE_CURRENT_SOURCE_DIR}/md4c")

add_library(md4c STATIC "${_md4c}/src/md4c.c")
add_library(hungryeditor::md4c ALIAS md4c)

target_include_directories(md4c SYSTEM PUBLIC "${_md4c}/src")

set_target_properties(md4c PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(md4c PRIVATE /w)
else()
    target_compile_options(md4c PRIVATE -w)
endif()
