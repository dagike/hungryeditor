// hungryeditor — a fast native Markdown editor.
//
// Placeholder entry point (commit 0.1). The Qt6 application and main window
// are introduced in commit 0.4.

#include <cstdio>

#ifndef HUNGRYEDITOR_VERSION
#define HUNGRYEDITOR_VERSION "0.0.0"
#endif

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    std::puts("hungryeditor " HUNGRYEDITOR_VERSION);
    return 0;
}
