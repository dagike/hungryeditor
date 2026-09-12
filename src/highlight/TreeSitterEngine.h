#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <tree_sitter/api.h>

namespace hungryeditor {

/// Owns a tree-sitter parser and the current syntax tree for a single
/// document. The engine keeps its own UTF-8 copy of the source so that
/// incremental reparses can reuse unchanged subtrees.
///
/// This type knows nothing about Scintilla or Qt; that wiring lives in
/// Editor, which owns a HighlightController/HighlightWorker pair driving an
/// instance of this class on a background thread.
class TreeSitterEngine
{
public:
    TreeSitterEngine();
    ~TreeSitterEngine();

    TreeSitterEngine(const TreeSitterEngine&) = delete;
    TreeSitterEngine& operator=(const TreeSitterEngine&) = delete;
    TreeSitterEngine(TreeSitterEngine&& other) noexcept;
    TreeSitterEngine& operator=(TreeSitterEngine&& other) noexcept;

    /// Select the grammar. Discards any existing tree.
    void setLanguage(const TSLanguage* language);
    const TSLanguage* language() const { return language_; }

    /// Replace the whole document and parse it from scratch.
    void setText(std::string source);

    /// Apply a single edit and incrementally reparse. `newSource` is the full
    /// document text after the edit. `edit` describes the changed byte range
    /// and its row/column extents (see pointAt()). Equivalent to
    /// noteEdit(edit) followed by reparse(newSource); kept for the common
    /// one-edit case and as this type's original API.
    void applyEdit(const TSInputEdit& edit, std::string newSource);

    /// Record one edit's byte/point deltas against the current tree, without
    /// reparsing yet (a no-op if there is no tree). Call once per edit, in
    /// the order the edits happened, then reparse() once at the end against
    /// the final text — tree-sitter's own pattern for folding several edits
    /// (e.g. two that landed within one highlight-debounce window) into a
    /// single incremental reparse instead of one full reparse per edit.
    void noteEdit(const TSInputEdit& edit);

    /// Reparse against `newSource`, reusing whatever edits were noted via
    /// noteEdit() since the last parse — a plain full parse if none were, or
    /// if there is no tree yet.
    void reparse(std::string newSource);

    bool hasTree() const { return tree_ != nullptr; }
    /// Root of the current tree. Only valid when hasTree().
    TSNode rootNode() const;

    std::string_view source() const { return source_; }

    /// Row/column (both zero-based, columns counted in bytes) of a byte
    /// offset within `text` — the coordinate space tree-sitter edits use.
    static TSPoint pointAt(std::string_view text, uint32_t byteOffset);

    /// Row/column reached after walking `span`'s bytes starting from
    /// `start` — `start` plus `span`'s own newline count and trailing-line
    /// length. Lets a TSInputEdit's end point be computed from just the
    /// bytes that changed (what Scintilla's own modification notification
    /// already carries) instead of re-scanning the whole document the way a
    /// pointAt() call from byte 0 would.
    static TSPoint pointAfter(TSPoint start, std::string_view span);

private:
    void reset() noexcept;

    TSParser* parser_ = nullptr;
    TSTree* tree_ = nullptr;
    const TSLanguage* language_ = nullptr;
    std::string source_;
};

} // namespace hungryeditor
