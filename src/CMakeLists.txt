# The real application. For commit 0.1 this is a placeholder entry point;
# the Qt6 main window arrives in commit 0.4.

add_executable(hungryeditor
    app/main.cpp
)

target_include_directories(hungryeditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(hungryeditor PRIVATE
    HUNGRYEDITOR_VERSION="${PROJECT_VERSION}"
)

hungryeditor_set_warnings(hungryeditor)
