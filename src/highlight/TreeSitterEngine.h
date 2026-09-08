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
/// This type knows nothing about Scintilla or Qt; the editor wiring lives in
/// a later commit.
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
    /// and its row/column extents (see pointAt()).
    void applyEdit(const TSInputEdit& edit, std::string newSource);

    bool hasTree() const { return tree_ != nullptr; }
    /// Root of the current tree. Only valid when hasTree().
    TSNode rootNode() const;

    std::string_view source() const { return source_; }

    /// Row/column (both zero-based, columns counted in bytes) of a byte
    /// offset within `text` — the coordinate space tree-sitter edits use.
    static TSPoint pointAt(std::string_view text, uint32_t byteOffset);

private:
    void reset() noexcept;

    TSParser* parser_ = nullptr;
    TSTree* tree_ = nullptr;
    const TSLanguage* language_ = nullptr;
    std::string source_;
};

} // namespace hungryeditor
