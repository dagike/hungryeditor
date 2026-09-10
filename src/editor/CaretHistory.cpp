#include "editor/CaretHistory.h"

#include <cstdlib>

namespace hungryeditor {

namespace {

bool sameSpot(const CaretLocation& a, const CaretLocation& b)
{
    return a.path == b.path && std::abs(a.line - b.line) <= CaretHistory::kMergeLines;
}

} // namespace

void CaretHistory::record(const CaretLocation& leaving)
{
    forward_.clear();

    if (!back_.empty() && sameSpot(back_.back(), leaving)) {
        back_.back() = leaving;
        return;
    }

    back_.push_back(leaving);
    if (static_cast<int>(back_.size()) > kMax) {
        back_.erase(back_.begin());
    }
}

CaretLocation CaretHistory::goBack(const CaretLocation& current)
{
    CaretLocation target = back_.back();
    back_.pop_back();
    forward_.push_back(current);
    return target;
}

CaretLocation CaretHistory::goForward(const CaretLocation& current)
{
    CaretLocation target = forward_.back();
    forward_.pop_back();
    back_.push_back(current);
    return target;
}

void CaretHistory::clear()
{
    back_.clear();
    forward_.clear();
}

} // namespace hungryeditor
