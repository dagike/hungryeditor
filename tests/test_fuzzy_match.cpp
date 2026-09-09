// Coverage for the fuzzy subsequence matcher.

#include <QtTest>

#include "ui/FuzzyMatch.h"

using hungryeditor::fuzzyMatch;

class TestFuzzyMatch : public QObject
{
    Q_OBJECT

private slots:
    void emptyPatternMatchesAnything();
    void matchesAnInOrderSubsequence();
    void rejectsOutOfOrderAndMissingChars();
    void boundaryAndConsecutiveMatchesScoreHigher();
};

void TestFuzzyMatch::emptyPatternMatchesAnything()
{
    QVERIFY(fuzzyMatch(QString(), QStringLiteral("whatever")).matched);
}

void TestFuzzyMatch::matchesAnInOrderSubsequence()
{
    QVERIFY(fuzzyMatch(QStringLiteral("fb"), QStringLiteral("FooBar")).matched);
    QVERIFY(fuzzyMatch(QStringLiteral("opre"), QStringLiteral("Open Recent")).matched);
    QVERIFY(fuzzyMatch(QStringLiteral("SAVE"), QStringLiteral("save")).matched); // case-insensitive
}

void TestFuzzyMatch::rejectsOutOfOrderAndMissingChars()
{
    QVERIFY(!fuzzyMatch(QStringLiteral("bf"), QStringLiteral("FooBar")).matched);
    QVERIFY(!fuzzyMatch(QStringLiteral("xyz"), QStringLiteral("abcdef")).matched);
    QVERIFY(!fuzzyMatch(QStringLiteral("fooo"), QStringLiteral("foo")).matched);
}

void TestFuzzyMatch::boundaryAndConsecutiveMatchesScoreHigher()
{
    const int wordStart = fuzzyMatch(QStringLiteral("op"), QStringLiteral("Open Path")).score;
    const int midWord = fuzzyMatch(QStringLiteral("op"), QStringLiteral("Loop")).score;
    QVERIFY(wordStart > midWord);

    const int consecutive = fuzzyMatch(QStringLiteral("com"), QStringLiteral("Command")).score;
    const int scattered = fuzzyMatch(QStringLiteral("com"), QStringLiteral("Close Menu")).score;
    QVERIFY(consecutive > scattered);
}

QTEST_APPLESS_MAIN(TestFuzzyMatch)
#include "test_fuzzy_match.moc"
