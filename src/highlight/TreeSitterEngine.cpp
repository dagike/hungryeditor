#include "highlight/TreeSitterEngine.h"

#include <cassert>
#include <utility>

namespace hungryeditor {

TreeSitterEngine::TreeSitterEngine() : parser_(ts_parser_new())
{
}

TreeSitterEngine::~TreeSitterEngine()
{
    if (tree_ != nullptr) {
        ts_tree_delete(tree_);
    }
    if (parser_ != nullptr) {
        ts_parser_delete(parser_);
    }
}

TreeSitterEngine::TreeSitterEngine(TreeSitterEngine&& other) noexcept
    : parser_(other.parser_), tree_(other.tree_), language_(other.language_),
      source_(std::move(other.source_))
{
    other.parser_ = nullptr;
    other.tree_ = nullptr;
    other.language_ = nullptr;
}

TreeSitterEngine& TreeSitterEngine::operator=(TreeSitterEngine&& other) noexcept
{
    if (this != &other) {
        reset();
        parser_ = other.parser_;
        tree_ = other.tree_;
        language_ = other.language_;
        source_ = std::move(other.source_);
        other.parser_ = nullptr;
        other.tree_ = nullptr;
        other.language_ = nullptr;
    }
    return *this;
}

void TreeSitterEngine::reset() noexcept
{
    if (tree_ != nullptr) {
        ts_tree_delete(tree_);
        tree_ = nullptr;
    }
    if (parser_ != nullptr) {
        ts_parser_delete(parser_);
        parser_ = nullptr;
    }
}

void TreeSitterEngine::setLanguage(const TSLanguage* language)
{
    language_ = language;
    ts_parser_set_language(parser_, language);
    if (tree_ != nullptr) {
        ts_tree_delete(tree_);
        tree_ = nullptr;
    }
}

void TreeSitterEngine::setText(std::string source)
{
    source_ = std::move(source);
    if (tree_ != nullptr) {
        ts_tree_delete(tree_);
        tree_ = nullptr;
    }
    if (language_ == nullptr) {
        return;
    }
    tree_ = ts_parser_parse_string(parser_, nullptr, source_.data(),
                                   static_cast<uint32_t>(source_.size()));
}

void TreeSitterEngine::applyEdit(const TSInputEdit& edit, std::string newSource)
{
    noteEdit(edit);
    reparse(std::move(newSource));
}

void TreeSitterEngine::noteEdit(const TSInputEdit& edit)
{
    if (tree_ != nullptr) {
        ts_tree_edit(tree_, &edit);
    }
}

void TreeSitterEngine::reparse(std::string newSource)
{
    source_ = std::move(newSource);

    if (language_ == nullptr) {
        if (tree_ != nullptr) {
            ts_tree_delete(tree_);
            tree_ = nullptr;
        }
        return;
    }

    // ts_parser_parse_string() accepts a null old tree as "nothing to
    // reuse, parse from scratch" — the same case setText() and the no-tree
    // branch this replaced both handled explicitly.
    TSTree* reparsed = ts_parser_parse_string(parser_, tree_, source_.data(),
                                              static_cast<uint32_t>(source_.size()));
    if (tree_ != nullptr) {
        ts_tree_delete(tree_);
    }
    tree_ = reparsed;
}

TSNode TreeSitterEngine::rootNode() const
{
    assert(tree_ != nullptr);
    return ts_tree_root_node(tree_);
}

TSPoint TreeSitterEngine::pointAt(std::string_view text, uint32_t byteOffset)
{
    const uint32_t limit =
        byteOffset < text.size() ? byteOffset : static_cast<uint32_t>(text.size());
    uint32_t row = 0;
    uint32_t column = 0;
    for (uint32_t i = 0; i < limit; ++i) {
        if (text[i] == '\n') {
            ++row;
            column = 0;
        } else {
            ++column;
        }
    }
    return TSPoint{row, column};
}

TSPoint TreeSitterEngine::pointAfter(TSPoint start, std::string_view span)
{
    TSPoint point = start;
    for (const char c : span) {
        if (c == '\n') {
            ++point.row;
            point.column = 0;
        } else {
            ++point.column;
        }
    }
    return point;
}

} // namespace hungryeditor
