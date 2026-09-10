#include "editor/MarkdownTable.h"

#include <algorithm>

namespace hungryeditor::mdtable {

namespace {

std::string trim(const std::string& s)
{
    const std::size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    return s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}

bool hasPipe(const std::string& s)
{
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\') {
            ++i;
            continue;
        }
        if (s[i] == '|') {
            return true;
        }
    }
    return false;
}

} // namespace

bool isDelimiterRow(const std::string& text)
{
    const std::vector<std::string> cells = splitCells(text);
    if (cells.empty()) {
        return false;
    }
    for (const std::string& cell : cells) {
        if (cell.empty()) {
            return false;
        }
        std::size_t i = 0;
        if (cell[i] == ':') {
            ++i;
        }
        const std::size_t dashStart = i;
        while (i < cell.size() && cell[i] == '-') {
            ++i;
        }
        if (i == dashStart) {
            return false; // no dashes
        }
        if (i < cell.size() && cell[i] == ':') {
            ++i;
        }
        if (i != cell.size()) {
            return false;
        }
    }
    return true;
}

TableRegion findTableRegion(const std::vector<std::string>& lines, int line)
{
    const int count = static_cast<int>(lines.size());
    const auto at = [&](int i) -> const std::string& { return lines[static_cast<std::size_t>(i)]; };
    if (line < 0 || line >= count || !hasPipe(at(line))) {
        return {};
    }

    int first = line;
    while (first > 0 && hasPipe(at(first - 1)) && !trim(at(first - 1)).empty()) {
        --first;
    }
    int last = line;
    while (last + 1 < count && hasPipe(at(last + 1)) && !trim(at(last + 1)).empty()) {
        ++last;
    }

    if (last - first < 1 || !isDelimiterRow(at(first + 1))) {
        return {};
    }
    return {first, last, true};
}

std::vector<std::string> splitCells(const std::string& row)
{
    std::string body = trim(row);
    if (!body.empty() && body.front() == '|') {
        body.erase(0, 1);
    }
    // Drop a trailing unescaped pipe.
    if (!body.empty() && body.back() == '|') {
        std::size_t backslashes = 0;
        for (std::size_t j = body.size() - 1; j > 0 && body[j - 1] == '\\'; --j) {
            ++backslashes;
        }
        if (backslashes % 2 == 0) {
            body.pop_back();
        }
    }

    std::vector<std::string> cells;
    std::string current;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '\\' && i + 1 < body.size() && body[i + 1] == '|') {
            current += '|';
            ++i;
            continue;
        }
        if (body[i] == '|') {
            cells.push_back(trim(current));
            current.clear();
            continue;
        }
        current += body[i];
    }
    cells.push_back(trim(current));
    return cells;
}

std::vector<ColumnAlign> parseAlignments(const std::string& delimiterRow)
{
    std::vector<ColumnAlign> aligns;
    for (const std::string& cell : splitCells(delimiterRow)) {
        const bool left = !cell.empty() && cell.front() == ':';
        const bool right = !cell.empty() && cell.back() == ':';
        if (left && right) {
            aligns.push_back(ColumnAlign::Center);
        } else if (right) {
            aligns.push_back(ColumnAlign::Right);
        } else if (left) {
            aligns.push_back(ColumnAlign::Left);
        } else {
            aligns.push_back(ColumnAlign::None);
        }
    }
    return aligns;
}

namespace {

std::string pad(const std::string& text, std::size_t width, ColumnAlign align)
{
    if (text.size() >= width) {
        return text;
    }
    const std::size_t slack = width - text.size();
    if (align == ColumnAlign::Right) {
        return std::string(slack, ' ') + text;
    }
    if (align == ColumnAlign::Center) {
        const std::size_t leftPad = slack / 2;
        return std::string(leftPad, ' ') + text + std::string(slack - leftPad, ' ');
    }
    return text + std::string(slack, ' ');
}

std::string delimiterCell(std::size_t width, ColumnAlign align)
{
    const bool left = align == ColumnAlign::Left || align == ColumnAlign::Center;
    const bool right = align == ColumnAlign::Right || align == ColumnAlign::Center;
    std::string cell;
    if (left) {
        cell += ':';
    }
    const std::size_t dashes = width - (left ? 1 : 0) - (right ? 1 : 0);
    cell += std::string(std::max<std::size_t>(dashes, 1), '-');
    if (right) {
        cell += ':';
    }
    return cell;
}

} // namespace

std::string renderAligned(const std::vector<std::vector<std::string>>& rows,
                          const std::vector<ColumnAlign>& aligns)
{
    std::size_t columns = aligns.size();
    for (const std::vector<std::string>& row : rows) {
        columns = std::max(columns, row.size());
    }
    if (columns == 0) {
        return {};
    }

    std::vector<std::size_t> widths(columns, 3);
    for (const std::vector<std::string>& row : rows) {
        for (std::size_t c = 0; c < row.size(); ++c) {
            widths[c] = std::max(widths[c], row[c].size());
        }
    }

    const auto alignAt = [&](std::size_t c) {
        return c < aligns.size() ? aligns[c] : ColumnAlign::None;
    };
    const auto renderRow = [&](const std::vector<std::string>& row) {
        std::string line = "|";
        for (std::size_t c = 0; c < columns; ++c) {
            const std::string cell = c < row.size() ? row[c] : std::string();
            line += ' ';
            line += pad(cell, widths[c], alignAt(c));
            line += " |";
        }
        return line;
    };

    std::string out;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        out += renderRow(rows[r]);
        out += '\n';
        if (r == 0) {
            std::string line = "|";
            for (std::size_t c = 0; c < columns; ++c) {
                line += ' ';
                line += delimiterCell(widths[c], alignAt(c));
                line += " |";
            }
            out += line;
            out += '\n';
        }
    }
    if (!out.empty()) {
        out.pop_back(); // drop the trailing newline
    }
    return out;
}

} // namespace hungryeditor::mdtable
