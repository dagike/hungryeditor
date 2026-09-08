# Test tree. The Qt Test harness proper is wired up in commit 0.6; for now
# there are two plain executables driven by CTest.

add_executable(smoke_test smoke_test.cpp)
target_compile_features(smoke_test PRIVATE cxx_std_20)
add_test(NAME smoke COMMAND smoke_test)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(vendor_link_test vendor_link_test.cpp)
target_link_libraries(vendor_link_test PRIVATE
    hungryeditor::scintilla
    hungryeditor::lexilla
    hungryeditor::treesitter
    Qt6::Widgets)
hungryeditor_set_warnings(vendor_link_test)
add_test(NAME vendor_link COMMAND vendor_link_test)

add_executable(main_window_test main_window_test.cpp)
set_target_properties(main_window_test PROPERTIES AUTOMOC ON)
target_link_libraries(main_window_test PRIVATE hungryeditor::core)
hungryeditor_set_warnings(main_window_test)
add_test(NAME main_window COMMAND main_window_test)
