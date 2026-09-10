#pragma once

#include <QString>

namespace hungryeditor {

/// Outcome of a fuzzy match. Higher `score` is a better match; only meaningful
/// when `matched` is true.
struct FuzzyResult
{
    bool matched = false;
    int score = 0;
};

/// Case-insensitive subsequence match of `pattern` against `text`. The score
/// rewards consecutive runs, matches at word boundaries (after a separator or
/// a camelCase hump) and an early first match. An empty pattern always matches
/// with score 0.
FuzzyResult fuzzyMatch(const QString& pattern, const QString& text);

} // namespace hungryeditor
