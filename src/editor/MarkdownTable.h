#pragma once

#include <string>
#include <vector>

/// Pure helpers for GFM pipe tables — no Qt, no Scintilla — so the parsing and
/// alignment rules can be unit-tested on their own. Editor drives them.
namespace hungryeditor::mdtable {

enum class ColumnAlign
{
    None,
    Left,
    Center,
    Right
};

/// A contiguous run of pipe-table lines, as 0-based indices into the line list.
struct TableRegion
{
    int firstLine = 0;
    int lastLine = 0;
    bool valid = false;
};

/// True when `text` is a GFM delimiter row: pipe-separated cells that are each
/// runs of `-` with optional leading/trailing `:` (e.g. `|:---|---:|:--:|`).
bool isDelimiterRow(const std::string& text);

/// The table containing line `line`: the run of adjacent lines that all carry a
/// `|`, valid only when its second line is a delimiter row and `line` is inside
/// the run.
TableRegion findTableRegion(const std::vector<std::string>& lines, int line);

/// Per-column alignments read from a delimiter row.
std::vector<ColumnAlign> parseAlignments(const std::string& delimiterRow);

/// One table row split into trimmed cell texts: the outer pipes are dropped and
/// a backslash-escaped `\|` stays a literal `|`.
std::vector<std::string> splitCells(const std::string& row);

/// Render content rows (row 0 is the header; the delimiter row is *not*
/// included) as an aligned pipe table, synthesising the delimiter row from
/// `aligns`. Columns are padded to their widest cell (minimum width 3).
std::string renderAligned(const std::vector<std::vector<std::string>>& rows,
                          const std::vector<ColumnAlign>& aligns);

} // namespace hungryeditor::mdtable
