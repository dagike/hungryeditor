#pragma once

#include <string>
#include <string_view>

#include <tree_sitter/api.h>

namespace hungryeditor::predicates {

/// True if `match` — an instance of `query`'s pattern `match.pattern_index`
/// — satisfies every filtering predicate attached to that pattern: `#eq?`,
/// `#not-eq?`, `#match?`, `#not-match?`, `#any-of?` and `#not-any-of?`,
/// evaluated against `source`, the text the match's captured nodes point
/// into (their byte offsets must be relative to it — for an injected
/// sub-tree that is the injected snippet's own text, not the outer
/// document's).
///
/// Directives with no defined filtering meaning here — `#set!` (used
/// separately, see directiveLanguage()) and the `#is?`/`#is-not?`
/// local-variable-scope hints a few grammars carry, which would need a
/// locals.scm scope-tracking pass this highlighter doesn't have — are
/// metadata, not filters: they never cause a match to be rejected. An
/// unrecognised directive name, or one used with an argument count this
/// evaluator doesn't handle, degrades the same way, so an upstream query
/// using something unsupported loses only that one directive's filtering,
/// never the capture outright.
bool matchesPredicates(const TSQuery* query, const TSQueryMatch& match, std::string_view source);

/// Value of a `(#set! injection.language "x")` directive on pattern
/// `patternIndex`, or empty when the pattern carries no such directive.
std::string directiveLanguage(const TSQuery* query, uint32_t patternIndex);

} // namespace hungryeditor::predicates
