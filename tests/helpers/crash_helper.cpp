// Deliberately crashes after installing the crash reporter — a separate
// process so the actual test (test_crash_reporter.cpp) can safely observe a
// real signal/exception without taking the test binary down with it.
//
// Usage: crash_helper <directory>

#include <QCoreApplication>
#include <QString>

#include "diagnostics/CrashReporter.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("test-version"));
    if (argc < 2) {
        return 2;
    }

    hungryeditor::crashreporter::install(QString::fromLocal8Bit(argv[1]));

    volatile int* null = nullptr;
    *null = 1; // SIGSEGV / access violation, on purpose
    return 0;  // unreachable
}
