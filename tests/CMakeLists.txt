# Test tree — Qt Test cases registered with CTest.
#
# Every test runs headless: hungryeditor_add_test() forces the "offscreen"
# QPA platform so the suite needs no display on CI or over SSH.

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

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

hungryeditor_add_test(test_vendor
    SOURCES test_vendor.cpp
    LIBS hungryeditor::scintilla hungryeditor::lexilla hungryeditor::treesitter Qt6::Widgets)

hungryeditor_add_test(test_editor
    SOURCES test_editor.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_treesitter_engine
    SOURCES test_treesitter_engine.cpp
    LIBS hungryeditor::core)

hungryeditor_add_test(test_main_window
    SOURCES test_main_window.cpp
    LIBS hungryeditor::core)
