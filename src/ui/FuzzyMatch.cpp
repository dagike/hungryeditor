#include "ui/FuzzyMatch.h"

#include <algorithm>

namespace hungryeditor {

namespace {

bool isSeparator(QChar c)
{
    return c == QLatin1Char(' ') || c == QLatin1Char('-') || c == QLatin1Char('_') ||
           c == QLatin1Char('.') || c == QLatin1Char('/') || c == QLatin1Char('\\');
}

constexpr int kConsecutiveBonus = 15;
constexpr int kBoundaryBonus = 30;
constexpr int kFirstCharBonus = 15;
constexpr int kUnmatchedPenalty = 1;

} // namespace

FuzzyResult fuzzyMatch(const QString& pattern, const QString& text)
{
    if (pattern.isEmpty()) {
        return {true, 0};
    }

    const QString lowerPattern = pattern.toLower();
    const QString lowerText = text.toLower();

    int score = 0;
    int patternIndex = 0;
    int previousMatch = -2;
    int firstMatch = -1;

    for (int i = 0; i < lowerText.size() && patternIndex < lowerPattern.size(); ++i) {
        if (lowerText.at(i) != lowerPattern.at(patternIndex)) {
            continue;
        }

        if (firstMatch < 0) {
            firstMatch = i;
        }
        if (i == previousMatch + 1) {
            score += kConsecutiveBonus;
        }

        const bool afterSeparator = i == 0 || isSeparator(text.at(i - 1));
        const bool camelHump = i > 0 && text.at(i).isUpper() && !text.at(i - 1).isUpper();
        if (afterSeparator || camelHump) {
            score += kBoundaryBonus;
        }
        if (i == 0) {
            score += kFirstCharBonus;
        }

        previousMatch = i;
        ++patternIndex;
    }

    if (patternIndex != lowerPattern.size()) {
        return {false, 0};
    }

    score += std::max(0, kFirstCharBonus - firstMatch);
    score -= kUnmatchedPenalty * int(lowerText.size() - lowerPattern.size());
    return {true, score};
}

} // namespace hungryeditor
