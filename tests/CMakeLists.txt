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
    Qt6::Widgets)
hungryeditor_set_warnings(vendor_link_test)
add_test(NAME vendor_link COMMAND vendor_link_test)
