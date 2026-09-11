// Randomized coverage for the Markdown renderer against malformed input.
//
// This is not a coverage-guided fuzzer (no libFuzzer/AFL, no corpus, no
// sanitizer build) — that is a separate infrastructure project this repo has
// nothing to build on yet. Instead this drives Md4cRenderer::toHtml() with a
// large, deterministic set of randomly-assembled malformed documents and
// checks for the two failure modes fuzzing actually catches: a crash (the
// process dies) and a hang or catastrophic slowdown (the elapsed-time check
// below, backstopped by this test's own CTest TIMEOUT). Fine-grained
// semantic correctness is already covered by the golden-file cases in
// test_md4c_renderer.cpp.

#include <random>

#include <QElapsedTimer>
#include <QString>
#include <QtTest>

#include "markdown/Md4cRenderer.h"

using hungryeditor::Md4cRenderer;

namespace {

// A fixed seed keeps every run — local or CI, any platform — reproducible:
// a failure here should be re-runnable to a specific document, not a
// once-in-CI ghost.
constexpr quint32 kSeed = 0xC0FFEE;
constexpr int kIterations = 3000;

QString randomWord(std::mt19937& rng)
{
    static const QStringList words = {
        QStringLiteral("lorem"),  QStringLiteral("ipsum"), QStringLiteral("héllo"),
        QStringLiteral("日本語"), QStringLiteral("a"),     QStringLiteral("emoji😀"),
        QStringLiteral("x"),
    };
    std::uniform_int_distribution<std::size_t> pick(0, std::size_t(words.size() - 1));
    return words.at(int(pick(rng)));
}

QString unterminatedFence(std::mt19937& rng)
{
    static const QStringList langs = {QStringLiteral("cpp"), QStringLiteral("rust"),
                                      QStringLiteral("bogus-lang"), QString()};
    std::uniform_int_distribution<std::size_t> langPick(0, std::size_t(langs.size() - 1));
    std::uniform_int_distribution<int> lines(0, 20);
    QString text = QStringLiteral("```%1\n").arg(langs.at(int(langPick(rng))));
    const int lineCount = lines(rng);
    for (int i = 0; i < lineCount; ++i) {
        text += randomWord(rng) + QStringLiteral(" ") + randomWord(rng) + QStringLiteral("\n");
    }
    // Deliberately no closing fence, some of the time.
    std::uniform_int_distribution<int> closeIt(0, 1);
    if (closeIt(rng) != 0) {
        text += QStringLiteral("``\n"); // one backtick short — still malformed
    }
    return text;
}

QString unbalancedBrackets(std::mt19937& rng)
{
    static const QStringList tokens = {QStringLiteral("["), QStringLiteral("]"),
                                       QStringLiteral("("), QStringLiteral(")"),
                                       QStringLiteral("!"), QStringLiteral("*")};
    std::uniform_int_distribution<std::size_t> pick(0, std::size_t(tokens.size() - 1));
    std::uniform_int_distribution<int> count(1, 40);
    QString text;
    const int n = count(rng);
    for (int i = 0; i < n; ++i) {
        text += tokens.at(int(pick(rng)));
        if (i % 3 == 0) {
            text += randomWord(rng);
        }
    }
    return text + QStringLiteral("\n");
}

QString nestedBlockquotesOrLists(std::mt19937& rng)
{
    std::uniform_int_distribution<int> depthDist(1, 15);
    std::uniform_int_distribution<int> markerDist(0, 2);
    QString text;
    const int depth = depthDist(rng);
    for (int i = 0; i < depth; ++i) {
        switch (markerDist(rng)) {
        case 0:
            text += QStringLiteral(">").repeated(i + 1) + QStringLiteral(" ");
            break;
        case 1:
            text += QStringLiteral(" ").repeated(i * 2) + QStringLiteral("- ");
            break;
        default:
            text += QStringLiteral(" ").repeated(i) + QStringLiteral("1. ");
            break;
        }
        text += randomWord(rng) + QStringLiteral("\n");
    }
    return text;
}

QString mismatchedEmphasis(std::mt19937& rng)
{
    static const QStringList markers = {QStringLiteral("*"),  QStringLiteral("**"),
                                        QStringLiteral("_"),  QStringLiteral("__"),
                                        QStringLiteral("~~"), QStringLiteral("`")};
    std::uniform_int_distribution<std::size_t> pick(0, std::size_t(markers.size() - 1));
    std::uniform_int_distribution<int> count(1, 30);
    QString text;
    const int n = count(rng);
    for (int i = 0; i < n; ++i) {
        text += markers.at(int(pick(rng)));
        if (i % 2 == 0) {
            text += randomWord(rng);
        }
    }
    return text + QStringLiteral("\n");
}

QString rawControlBytes(std::mt19937& rng)
{
    std::uniform_int_distribution<int> controlChar(0, 31);
    std::uniform_int_distribution<int> count(1, 20);
    QString text;
    const int n = count(rng);
    for (int i = 0; i < n; ++i) {
        const int c = controlChar(rng);
        if (c == '\n' || c == '\t') {
            continue; // real newlines/tabs are not "malformed" on their own
        }
        text += QChar(c);
    }
    return text + QStringLiteral("\n");
}

QString unpairedSurrogates(std::mt19937& rng)
{
    std::uniform_int_distribution<int> pick(0, 1);
    QString text;
    // 0xD800-0xDBFF (high) / 0xDC00-0xDFFF (low), each alone — deliberately
    // never paired, which is invalid UTF-16 but a QString does not forbid it.
    text += pick(rng) == 0 ? QChar(0xD800) : QChar(0xDC00);
    text += randomWord(rng);
    return text + QStringLiteral("\n");
}

QString malformedFrontMatter(std::mt19937& rng)
{
    std::uniform_int_distribution<int> closeIt(0, 1);
    std::uniform_int_distribution<int> fieldCount(0, 5);
    QString text = QStringLiteral("---\n");
    const int n = fieldCount(rng);
    for (int i = 0; i < n; ++i) {
        text += randomWord(rng) + QStringLiteral(": ") + randomWord(rng) + QStringLiteral("\n");
    }
    if (closeIt(rng) != 0) {
        text += QStringLiteral("---\n");
    }
    // Never closed, the rest of the time — the parse() contract explicitly
    // allows that (present == false), exercised here at the renderer level.
    return text;
}

QString malformedFootnotes(std::mt19937& rng)
{
    std::uniform_int_distribution<int> refCount(1, 10);
    std::uniform_int_distribution<int> defineIt(0, 1);
    QString text;
    const int n = refCount(rng);
    for (int i = 0; i < n; ++i) {
        const QString id = QStringLiteral("id%1").arg(i);
        text += QStringLiteral("[^%1] ").arg(id);
        if (defineIt(rng) != 0) {
            text += QStringLiteral("\n[^%1]: %2\n").arg(id, randomWord(rng));
        }
    }
    return text + QStringLiteral("\n");
}

QString longRepeatedRun(std::mt19937& rng)
{
    static const QStringList chars = {QStringLiteral("#"), QStringLiteral("*"), QStringLiteral("`"),
                                      QStringLiteral("-"), QStringLiteral(">")};
    std::uniform_int_distribution<std::size_t> pick(0, std::size_t(chars.size() - 1));
    std::uniform_int_distribution<int> length(100, 2000);
    return chars.at(int(pick(rng))).repeated(length(rng)) + QStringLiteral("\n");
}

QString tableSoup(std::mt19937& rng)
{
    std::uniform_int_distribution<int> rowCount(1, 8);
    std::uniform_int_distribution<int> colCount(1, 8);
    QString text;
    const int rows = rowCount(rng);
    for (int r = 0; r < rows; ++r) {
        const int cols = colCount(rng); // deliberately varies row to row
        for (int c = 0; c < cols; ++c) {
            text += QStringLiteral("|") + randomWord(rng);
        }
        text += QStringLiteral("|\n");
        if (r == 0) {
            text += QStringLiteral("|") + QStringLiteral("---|").repeated(cols);
            text += QStringLiteral("\n");
        }
    }
    return text;
}

QString mixedLineEndings(std::mt19937& rng)
{
    static const QStringList endings = {QStringLiteral("\n"), QStringLiteral("\r\n"),
                                        QStringLiteral("\r")};
    std::uniform_int_distribution<std::size_t> pick(0, std::size_t(endings.size() - 1));
    std::uniform_int_distribution<int> lineCount(1, 12);
    QString text;
    const int n = lineCount(rng);
    for (int i = 0; i < n; ++i) {
        text += randomWord(rng) + endings.at(int(pick(rng)));
    }
    return text;
}

using Generator = QString (*)(std::mt19937&);

const QVector<Generator>& generators()
{
    static const QVector<Generator> table = {
        unterminatedFence, unbalancedBrackets, nestedBlockquotesOrLists, mismatchedEmphasis,
        rawControlBytes,   unpairedSurrogates, malformedFrontMatter,     malformedFootnotes,
        longRepeatedRun,   tableSoup,          mixedLineEndings,
    };
    return table;
}

/// A malformed document assembled from a handful of randomly-picked,
/// randomly-ordered generators — a real document is rarely just one kind of
/// broken.
QString randomMalformedDocument(std::mt19937& rng)
{
    const QVector<Generator>& gens = generators();
    std::uniform_int_distribution<std::size_t> genPick(0, std::size_t(gens.size() - 1));
    std::uniform_int_distribution<int> partCount(1, 4);

    QString text;
    const int parts = partCount(rng);
    for (int i = 0; i < parts; ++i) {
        text += gens.at(int(genPick(rng)))(rng);
        text += QStringLiteral("\n");
    }
    return text;
}

} // namespace

class TestMd4cFuzz : public QObject
{
    Q_OBJECT

private slots:
    void survivesRandomMalformedDocuments();
};

void TestMd4cFuzz::survivesRandomMalformedDocuments()
{
    std::mt19937 rng(kSeed);
    Md4cRenderer renderer;

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < kIterations; ++i) {
        const QString document = randomMalformedDocument(rng);
        const QString html = renderer.toHtml(document);

        // Unbounded blowup (not just ordinary tag overhead) is itself a bug
        // worth catching, independent of the timing guard below.
        QVERIFY2(html.size() < document.size() * 50 + 10000,
                 qPrintable(QStringLiteral("iteration %1: input %2 chars produced %3 chars of html")
                                .arg(i)
                                .arg(document.size())
                                .arg(html.size())));
    }

    // A generous total-time guard: any single catastrophically slow input
    // among kIterations ordinary-sized documents pushes this well past a
    // normal run (each document is at most a few KB). The CTest TIMEOUT on
    // this executable (see CMakeLists.txt) is the hard backstop against a
    // genuine infinite hang.
    QVERIFY2(timer.elapsed() < 30000,
             qPrintable(
                 QStringLiteral("%1 iterations took %2 ms").arg(kIterations).arg(timer.elapsed())));
}

QTEST_APPLESS_MAIN(TestMd4cFuzz)
#include "test_md4c_fuzz.moc"
