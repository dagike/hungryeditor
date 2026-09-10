#pragma once

#include <vector>

#include <QString>

namespace hungryeditor {

/// A caret position worth returning to: a file (empty ⇒ the current untitled
/// buffer) and a zero-based line/column.
struct CaretLocation
{
    QString path;
    int line = 0;
    int column = 0;
};

/// Browser-style back/forward history of caret positions. The window records
/// the spot it is leaving before each deliberate jump (outline click, preview
/// heading, search result, go-to-line, quick open); Back returns there and
/// stacks the current spot for Forward. A new record after going Back drops the
/// forward tail.
class CaretHistory
{
public:
    static constexpr int kMax = 60;       ///< most positions kept on the back stack
    static constexpr int kMergeLines = 1; ///< records this close to the last coalesce

    /// Note the position being left. Coalesces with the previous record at the
    /// same spot and clears the forward stack.
    void record(const CaretLocation& leaving);

    bool canGoBack() const { return !back_.empty(); }
    bool canGoForward() const { return !forward_.empty(); }

    /// Pop the most recent back position, pushing `current` for Forward.
    /// Callers must check canGoBack() first.
    CaretLocation goBack(const CaretLocation& current);
    /// Inverse of goBack(). Callers must check canGoForward() first.
    CaretLocation goForward(const CaretLocation& current);

    void clear();

    /// Total positions held (back + forward). For tests.
    int size() const { return static_cast<int>(back_.size() + forward_.size()); }

private:
    std::vector<CaretLocation> back_;
    std::vector<CaretLocation> forward_;
};

} // namespace hungryeditor
