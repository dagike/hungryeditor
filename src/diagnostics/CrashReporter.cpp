#include "diagnostics/CrashReporter.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iterator>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QtGlobal>

#if defined(Q_OS_UNIX)
#include <csignal>

#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
// Without this, <windows.h> defines max/min macros that mangle the
// std::min() call in copyToFixedBuffer() below into invalid syntax (MSVC
// C2589: "illegal token on right side of '::'").
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace hungryeditor::crashreporter {

namespace {

constexpr std::size_t kPathCap = 4096;
constexpr std::size_t kVersionCap = 128;
constexpr int kMaxFrames = 64;

// Fixed-size, written once from install() (ordinary, non-crashing code) and
// only ever read afterward from the handler — never allocated or mutated
// there, since heap allocation and QString are not safe to touch from a
// POSIX signal handler.
char g_crashDir[kPathCap] = {};
char g_appVersion[kVersionCap] = {};
char g_qtVersion[kVersionCap] = {};
bool g_installed = false;

#if defined(Q_OS_UNIX)
constexpr int kHandledSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};
struct sigaction g_previousActions[std::size(kHandledSignals)];
constexpr std::size_t kAltStackSize = 65536; // generous for backtrace() + a few snprintf calls
char g_altStackMemory[kAltStackSize];

const char* signalName(int sig)
{
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV";
    case SIGABRT:
        return "SIGABRT";
    case SIGFPE:
        return "SIGFPE";
    case SIGILL:
        return "SIGILL";
    case SIGBUS:
        return "SIGBUS";
    default:
        return "UNKNOWN";
    }
}

// Every call here is async-signal-safe: snprintf into a stack buffer with
// only integer/string conversions, open/write/close, and
// backtrace_symbols_fd (glibc's documented signal-safe variant — unlike
// backtrace_symbols(), it writes directly to an fd instead of allocating).
void writeReport(int sig)
{
    // Comfortably larger than kPathCap so appending "/crashes/crash-<secs>.txt"
    // can never truncate g_crashDir, however close to its own cap it is.
    char path[kPathCap + 64];
    const std::time_t now = std::time(nullptr);
    std::snprintf(path, sizeof(path), "%s/crashes/crash-%lld.txt", g_crashDir,
                  static_cast<long long>(now));

    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }

    struct tm timeBuf
    {
    };
    gmtime_r(&now, &timeBuf);

    char header[512];
    const int len = std::snprintf(
        header, sizeof(header),
        "hungryeditor crash report\nsignal: %s\ntime (UTC): %04d-%02d-%02dT%02d:%02d:%02dZ\n"
        "app version: %s\nqt version: %s\n\nbacktrace:\n",
        signalName(sig), timeBuf.tm_year + 1900, timeBuf.tm_mon + 1, timeBuf.tm_mday,
        timeBuf.tm_hour, timeBuf.tm_min, timeBuf.tm_sec, g_appVersion, g_qtVersion);
    if (len > 0) {
        (void) write(fd, header, static_cast<std::size_t>(len));
    }

    void* frames[kMaxFrames];
    const int frameCount = backtrace(frames, kMaxFrames);
    backtrace_symbols_fd(frames, frameCount, fd);

    close(fd);
}

void handleFatalSignal(int sig, siginfo_t* /*info*/, void* /*context*/)
{
    writeReport(sig);

    // Put back whatever was installed before us (a debugger, a sanitizer, or
    // the platform default) and let the crash proceed exactly as it would
    // have without this handler — nothing here is swallowed.
    for (std::size_t i = 0; i < std::size(kHandledSignals); ++i) {
        if (kHandledSignals[i] == sig) {
            sigaction(sig, &g_previousActions[i], nullptr);
            break;
        }
    }
    raise(sig);
}
#elif defined(Q_OS_WIN)
LONG WINAPI handleUnhandledException(EXCEPTION_POINTERS* info)
{
    // See the POSIX writeReport() above for why this is larger than kPathCap.
    char path[kPathCap + 64];
    const std::time_t now = std::time(nullptr);
    std::snprintf(path, sizeof(path), "%s/crashes/crash-%lld.txt", g_crashDir,
                  static_cast<long long>(now));

    HANDLE file =
        CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        const unsigned long code =
            (info != nullptr && info->ExceptionRecord != nullptr)
                ? static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode)
                : 0UL;
        char header[512];
        const int len = std::snprintf(
            header, sizeof(header),
            "hungryeditor crash report\nexception code: 0x%08lX\napp version: %s\nqt version: "
            "%s\n\nbacktrace (raw addresses — resolve offline against this build):\n",
            code, g_appVersion, g_qtVersion);
        DWORD written = 0;
        if (len > 0) {
            WriteFile(file, header, static_cast<DWORD>(len), &written, nullptr);
        }

        void* frames[kMaxFrames];
        const USHORT frameCount = CaptureStackBackTrace(0, kMaxFrames, frames, nullptr);
        for (USHORT i = 0; i < frameCount; ++i) {
            char line[64];
            const int lineLen =
                std::snprintf(line, sizeof(line), "  #%u %p\n", unsigned(i), frames[i]);
            if (lineLen > 0) {
                WriteFile(file, line, static_cast<DWORD>(lineLen), &written, nullptr);
            }
        }
        CloseHandle(file);
    }

    // Let any previously-installed filter (a debugger, typically) still run.
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void copyToFixedBuffer(char* dest, std::size_t capacity, const QByteArray& source)
{
    const std::size_t length = std::min(static_cast<std::size_t>(source.size()), capacity - 1);
    std::memcpy(dest, source.constData(), length);
    dest[length] = '\0';
}

} // namespace

void install(const QString& directory)
{
    if (g_installed) {
        return;
    }
    g_installed = true;

    QDir().mkpath(directory + QStringLiteral("/crashes"));

    copyToFixedBuffer(g_crashDir, kPathCap, directory.toUtf8());
    copyToFixedBuffer(g_appVersion, kVersionCap, QCoreApplication::applicationVersion().toUtf8());
    copyToFixedBuffer(g_qtVersion, kVersionCap, QByteArray(qVersion()));

#if defined(Q_OS_UNIX)
    stack_t altStack{};
    altStack.ss_sp = g_altStackMemory;
    altStack.ss_size = kAltStackSize;
    altStack.ss_flags = 0;
    sigaltstack(&altStack, nullptr);

    struct sigaction action
    {
    };
    action.sa_sigaction = handleFatalSignal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    for (std::size_t i = 0; i < std::size(kHandledSignals); ++i) {
        sigaction(kHandledSignals[i], &action, &g_previousActions[i]);
    }
#elif defined(Q_OS_WIN)
    SetUnhandledExceptionFilter(handleUnhandledException);
#endif
}

} // namespace hungryeditor::crashreporter
