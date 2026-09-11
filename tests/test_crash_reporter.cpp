// Coverage for the opt-in local crash reporter. A real SIGSEGV is raised in
// a separate helper process (see helpers/crash_helper.cpp) so this test
// itself never crashes — it only observes the report the helper's crash
// left behind.

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#ifndef CRASH_HELPER_PATH
#error "CRASH_HELPER_PATH must be defined by CMake"
#endif

class TestCrashReporter : public QObject
{
    Q_OBJECT

private slots:
    void writesAReportOnACrash();
};

void TestCrashReporter::writesAReportOnACrash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QProcess process;
    process.start(QStringLiteral(CRASH_HELPER_PATH), {dir.path()});
    QVERIFY(process.waitForFinished(10000));
    QVERIFY2(process.exitStatus() == QProcess::CrashExit,
             "the helper should have been killed by its own deliberate SIGSEGV");

    const QDir crashesDir(dir.filePath(QStringLiteral("crashes")));
    const QStringList reports = crashesDir.entryList(QDir::Files);
    QCOMPARE(reports.size(), 1);

    QFile report(crashesDir.filePath(reports.first()));
    QVERIFY(report.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(report.readAll());

    QVERIFY(contents.contains(QStringLiteral("SIGSEGV")));
    QVERIFY(contents.contains(QStringLiteral("test-version")));
    QVERIFY(contents.contains(QStringLiteral("backtrace:")));
    // The backtrace itself: at least one line beyond the header, from
    // backtrace_symbols_fd().
    QVERIFY(contents.split(QStringLiteral("backtrace:\n")).last().trimmed().size() > 0);
}

QTEST_MAIN(TestCrashReporter)
#include "test_crash_reporter.moc"
