#include "highlight/GrammarRegistry.h"

#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "HighlightQueries.h" // generated: hungryeditor::queries::*

// clang-format off
extern "C" {
const TSLanguage* tree_sitter_bash(void);
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_cpp(void);
const TSLanguage* tree_sitter_c_sharp(void);
const TSLanguage* tree_sitter_css(void);
const TSLanguage* tree_sitter_go(void);
const TSLanguage* tree_sitter_html(void);
const TSLanguage* tree_sitter_java(void);
const TSLanguage* tree_sitter_javascript(void);
const TSLanguage* tree_sitter_json(void);
const TSLanguage* tree_sitter_php(void);
const TSLanguage* tree_sitter_python(void);
const TSLanguage* tree_sitter_ruby(void);
const TSLanguage* tree_sitter_rust(void);
const TSLanguage* tree_sitter_toml(void);
const TSLanguage* tree_sitter_typescript(void);
const TSLanguage* tree_sitter_tsx(void);
const TSLanguage* tree_sitter_yaml(void);
}
// clang-format on

namespace hungryeditor {

namespace {

struct Entry
{
    std::string_view alias;
    const TSLanguage* language;
    std::string_view highlights;
};

// One row per accepted name; aliases repeat the grammar and query. Names are
// lowercase — the lookup lowercases its input before comparing. Built once on
// first use so the grammar entry points are resolved at run time.
const std::vector<Entry>& entries()
{
    static const std::vector<Entry> table = {
        {"bash", tree_sitter_bash(), queries::kLangBash},
        {"sh", tree_sitter_bash(), queries::kLangBash},
        {"shell", tree_sitter_bash(), queries::kLangBash},
        {"zsh", tree_sitter_bash(), queries::kLangBash},
        {"c", tree_sitter_c(), queries::kLangC},
        {"h", tree_sitter_c(), queries::kLangC},
        {"cpp", tree_sitter_cpp(), queries::kLangCpp},
        {"c++", tree_sitter_cpp(), queries::kLangCpp},
        {"cc", tree_sitter_cpp(), queries::kLangCpp},
        {"cxx", tree_sitter_cpp(), queries::kLangCpp},
        {"hpp", tree_sitter_cpp(), queries::kLangCpp},
        {"cs", tree_sitter_c_sharp(), queries::kLangCSharp},
        {"csharp", tree_sitter_c_sharp(), queries::kLangCSharp},
        {"c#", tree_sitter_c_sharp(), queries::kLangCSharp},
        {"css", tree_sitter_css(), queries::kLangCss},
        {"go", tree_sitter_go(), queries::kLangGo},
        {"golang", tree_sitter_go(), queries::kLangGo},
        {"html", tree_sitter_html(), queries::kLangHtml},
        {"htm", tree_sitter_html(), queries::kLangHtml},
        {"java", tree_sitter_java(), queries::kLangJava},
        {"javascript", tree_sitter_javascript(), queries::kLangJavaScript},
        {"js", tree_sitter_javascript(), queries::kLangJavaScript},
        {"jsx", tree_sitter_javascript(), queries::kLangJavaScript},
        {"mjs", tree_sitter_javascript(), queries::kLangJavaScript},
        {"cjs", tree_sitter_javascript(), queries::kLangJavaScript},
        {"json", tree_sitter_json(), queries::kLangJson},
        {"jsonc", tree_sitter_json(), queries::kLangJson},
        {"php", tree_sitter_php(), queries::kLangPhp},
        {"python", tree_sitter_python(), queries::kLangPython},
        {"py", tree_sitter_python(), queries::kLangPython},
        {"ruby", tree_sitter_ruby(), queries::kLangRuby},
        {"rb", tree_sitter_ruby(), queries::kLangRuby},
        {"rust", tree_sitter_rust(), queries::kLangRust},
        {"rs", tree_sitter_rust(), queries::kLangRust},
        {"toml", tree_sitter_toml(), queries::kLangToml},
        {"typescript", tree_sitter_typescript(), queries::kLangTypeScript},
        {"ts", tree_sitter_typescript(), queries::kLangTypeScript},
        {"tsx", tree_sitter_tsx(), queries::kLangTypeScript},
        {"typescriptreact", tree_sitter_tsx(), queries::kLangTypeScript},
        {"yaml", tree_sitter_yaml(), queries::kLangYaml},
        {"yml", tree_sitter_yaml(), queries::kLangYaml},
    };
    return table;
}

} // namespace

Grammar grammarForName(std::string_view name)
{
    std::string key;
    key.reserve(name.size());
    for (const char c : name) {
        key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    for (const Entry& entry : entries()) {
        if (entry.alias == key) {
            return {entry.language, entry.highlights};
        }
    }
    return {};
}

TSQuery* cachedHighlightsQuery(const TSLanguage* language, std::string_view highlights)
{
    if (language == nullptr) {
        return nullptr;
    }

    static std::mutex mutex;
    static std::unordered_map<const TSLanguage*, TSQuery*> cache;

    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = cache.find(language);
    if (it != cache.end()) {
        return it->second;
    }

    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    // A malformed query simply disables that layer of highlighting; not fatal.
    TSQuery* query =
        ts_query_new(language, highlights.data(), static_cast<uint32_t>(highlights.size()),
                     &errorOffset, &errorType);
    cache.emplace(language, query);
    return query;
}

} // namespace hungryeditor
