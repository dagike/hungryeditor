# Test tree — Qt Test cases registered with CTest.
#
# Every test runs headless: hungryeditor_add_test() forces the "offscreen"
# QPA platform so the suite needs no display on CI or over SSH.

find_package(Qt6 REQUIRED COMPONENTS Test Widgets PrintSupport)

# hungryeditor_add_test(<name> SOURCES <files...> [LIBS <targets...>])
function(hungryeditor_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    hungryeditor_set_warnings(${name})

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# Tests that construct a Qt WebEngine view need Chromium to come up headless.
function(hungryeditor_test_needs_webengine name)
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT
        "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu --disable-dev-shm-usage")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "QTWEBENGINE_DISABLE_SANDBOX=1")
endfunction()

hungryeditor_add_test(test_vendor
    SOURCES test_vendor.cpp
    LIBS hungryeditor::scintilla hungryeditor::lexilla hungryeditor::treesitter Qt6::Widgets)

hungryeditor_add_test(test_editor
    SOURCES test_editor.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_treesitter_engine
    SOURCES test_treesitter_engine.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_highlight_worker
    SOURCES test_highlight_worker.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_grammar_registry
    SOURCES test_grammar_registry.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_command_line
    SOURCES test_command_line.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_text_file
    SOURCES test_text_file.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_draft_store
    SOURCES test_draft_store.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_session_store
    SOURCES test_session_store.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_recent_files
    SOURCES test_recent_files.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_asset_writer
    SOURCES test_asset_writer.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_markdown_table
    SOURCES test_markdown_table.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_caret_history
    SOURCES test_caret_history.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_document_manager
    SOURCES test_document_manager.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_main_window
    SOURCES test_main_window.cpp
    LIBS hungryeditor::core)
hungryeditor_test_needs_webengine(test_main_window)

hungryeditor_add_test(test_tab_bar
    SOURCES test_tab_bar.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_theme
    SOURCES test_theme.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_file_search
    SOURCES test_file_search.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_file_index
    SOURCES test_file_index.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_fuzzy_match
    SOURCES test_fuzzy_match.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_command_palette
    SOURCES test_command_palette.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_tab_switcher
    SOURCES test_tab_switcher.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_code_highlighter
    SOURCES test_code_highlighter.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_front_matter
    SOURCES test_front_matter.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_outline
    SOURCES test_outline.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_outline_panel
    SOURCES test_outline_panel.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_file_tree_panel
    SOURCES test_file_tree_panel.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_workspace_store
    SOURCES test_workspace_store.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_file_operations
    SOURCES test_file_operations.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_image_resolver
    SOURCES test_image_resolver.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_md4c_renderer
    SOURCES test_md4c_renderer.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_preview_controller
    SOURCES test_preview_controller.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_preview
    SOURCES test_preview.cpp
    LIBS hungryeditor::core Qt6::PrintSupport)
hungryeditor_test_needs_webengine(test_preview)

hungryeditor_add_test(test_html_document
    SOURCES test_html_document.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_theme_file
    SOURCES test_theme_file.cpp
    LIBS hungryeditor::core)
