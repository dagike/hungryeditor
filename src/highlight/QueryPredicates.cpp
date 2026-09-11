#include "highlight/QueryPredicates.h"

#include <regex>
#include <unordered_map>
#include <vector>

namespace hungryeditor::predicates {

namespace {

/// One argument to a predicate directive: either a capture's matched text
/// (resolved against a specific match) or a literal string from the query.
struct Arg
{
    bool isCapture = false;
    std::string_view text;
};

/// One `(#name? arg...)` directive attached to a pattern, with `name`
/// stripped of its leading `#` (tree-sitter's own predicate representation
/// already does this — it is never part of the string value).
struct Directive
{
    std::string_view name;
    std::vector<Arg> args;
};

/// Text captured by pattern-capture-index `captureIndex` within `match`, or
/// empty if that capture didn't participate in this particular match (an
/// optional capture that never fired).
std::string_view capturedText(const TSQueryMatch& match, uint32_t captureIndex,
                              std::string_view source)
{
    for (uint16_t i = 0; i < match.capture_count; ++i) {
        if (match.captures[i].index != captureIndex) {
            continue;
        }
        const TSNode node = match.captures[i].node;
        const uint32_t start = ts_node_start_byte(node);
        const uint32_t end = ts_node_end_byte(node);
        if (start <= end && end <= source.size()) {
            return source.substr(start, end - start);
        }
    }
    return {};
}

/// Every predicate directive attached to `query`'s pattern `patternIndex`,
/// with capture arguments already resolved against `match` and `source`.
std::vector<Directive> directivesForPattern(const TSQuery* query, uint32_t patternIndex,
                                            const TSQueryMatch& match, std::string_view source)
{
    uint32_t stepCount = 0;
    const TSQueryPredicateStep* steps =
        ts_query_predicates_for_pattern(query, patternIndex, &stepCount);

    std::vector<Directive> directives;
    Directive current;
    for (uint32_t i = 0; i < stepCount; ++i) {
        const TSQueryPredicateStep& step = steps[i];
        if (step.type == TSQueryPredicateStepTypeDone) {
            if (!current.name.empty() || !current.args.empty()) {
                directives.push_back(current);
            }
            current = Directive{};
            continue;
        }

        if (step.type == TSQueryPredicateStepTypeString) {
            uint32_t len = 0;
            const char* value = ts_query_string_value_for_id(query, step.value_id, &len);
            const std::string_view text(value, len);
            if (current.name.empty()) {
                current.name = text; // the predicate's own name is always first
            } else {
                current.args.push_back(Arg{false, text});
            }
        } else { // TSQueryPredicateStepTypeCapture
            current.args.push_back(Arg{true, capturedText(match, step.value_id, source)});
        }
    }
    return directives;
}

/// Cached `std::regex::search` against `pattern`. The same handful of
/// patterns (~30 across the vendored grammars, all plain anchored character
/// classes) recur on every match of the rule that carries them, so
/// compiling once per distinct pattern string — not once per match — avoids
/// the same wasteful-recompile shape GrammarRegistry::cachedHighlightsQuery()
/// fixed for tree-sitter queries in Phase 9. One cache per thread:
/// HighlightWorker and CodeHighlighter each run on their own thread and
/// never share this map.
bool regexSearch(std::string_view text, std::string_view pattern)
{
    thread_local std::unordered_map<std::string, std::regex> cache;
    const std::string key(pattern);
    auto it = cache.find(key);
    if (it == cache.end()) {
        try {
            it = cache.emplace(key, std::regex(key, std::regex::ECMAScript)).first;
        } catch (const std::regex_error&) {
            // A pattern this evaluator can't compile disables just that one
            // predicate — the same "degrade, don't crash" choice newQuery()
            // makes for a malformed highlights.scm elsewhere in this file.
            return false;
        }
    }
    return std::regex_search(text.begin(), text.end(), it->second);
}

bool evaluate(const Directive& directive)
{
    const std::string_view name = directive.name;
    const std::vector<Arg>& args = directive.args;

    if (name == "eq?" && args.size() == 2) {
        return args[0].text == args[1].text;
    }
    if (name == "not-eq?" && args.size() == 2) {
        return args[0].text != args[1].text;
    }
    if (name == "match?" && args.size() == 2) {
        return regexSearch(args[0].text, args[1].text);
    }
    if (name == "not-match?" && args.size() == 2) {
        return !regexSearch(args[0].text, args[1].text);
    }
    if (name == "any-of?" && args.size() >= 2) {
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[0].text == args[i].text) {
                return true;
            }
        }
        return false;
    }
    if (name == "not-any-of?" && args.size() >= 2) {
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[0].text == args[i].text) {
                return false;
            }
        }
        return true;
    }
    // #set!, #is?, #is-not?, an unrecognised name, or a recognised name used
    // with an argument count we don't handle: metadata, not a filter — see
    // matchesPredicates()'s doc comment.
    return true;
}

} // namespace

bool matchesPredicates(const TSQuery* query, const TSQueryMatch& match, std::string_view source)
{
    for (const Directive& directive :
         directivesForPattern(query, match.pattern_index, match, source)) {
        if (!evaluate(directive)) {
            return false;
        }
    }
    return true;
}

std::string directiveLanguage(const TSQuery* query, uint32_t patternIndex)
{
    uint32_t stepCount = 0;
    const TSQueryPredicateStep* steps =
        ts_query_predicates_for_pattern(query, patternIndex, &stepCount);

    std::vector<std::string_view> args;
    for (uint32_t i = 0; i < stepCount; ++i) {
        const TSQueryPredicateStep& step = steps[i];
        if (step.type == TSQueryPredicateStepTypeDone) {
            if (args.size() == 3 && args[0] == "set!" && args[1] == "injection.language") {
                return std::string(args[2]);
            }
            args.clear();
        } else if (step.type == TSQueryPredicateStepTypeString) {
            uint32_t len = 0;
            const char* value = ts_query_string_value_for_id(query, step.value_id, &len);
            args.emplace_back(value, len);
        } else {
            args.emplace_back(); // capture step — keep argument positions aligned
        }
    }
    return {};
}

} // namespace hungryeditor::predicates
