# Third-party components

All vendored sources live under `third_party/` and are kept as close to
upstream as practical — only build scripts and IDE project files were removed.

| Component | Version | Upstream | License |
|-----------|---------|----------|---------|
| Scintilla | 5.5.5 (`555`) | https://www.scintilla.org/ | HPND-style — `third_party/scintilla/License.txt` |
| Lexilla | 5.4.0 (`540`) | https://www.scintilla.org/Lexilla.html | HPND-style — `third_party/lexilla/License.txt` |
| tree-sitter | 0.24.7 | https://github.com/tree-sitter/tree-sitter | MIT — `third_party/tree-sitter/LICENSE` |
| tree-sitter-markdown | 0.4.1 | https://github.com/tree-sitter-grammars/tree-sitter-markdown | MIT — `third_party/tree-sitter-markdown/LICENSE` |

## Notes

- **Scintilla** is built with its Qt platform layer
  (`qt/ScintillaEditBase/`) and linked statically. It requires
  `Qt6::Core5Compat` for `QTextCodec` (legacy codepage handling).
- **Lexilla** provides the stock lexers used only on the large-file fallback
  path (files above the tree-sitter size limit). The full lexer catalogue is
  compiled; trimming it is a later size-optimization decision.
- **tree-sitter** is compiled from its amalgamated `lib/src/lib.c`; the Wasm
  feature is disabled and its sources were not vendored. The Markdown grammar
  ships two parsers — block (`tree_sitter_markdown`) and inline
  (`tree_sitter_markdown_inline`) — built from their pre-generated `parser.c`,
  so no `tree-sitter generate` / Node toolchain is needed. The grammars'
  `queries/*.scm` (highlight and injection rules) are kept for use from
  Phase 1 onward. Additional language grammars are added in commit 1.7.
- All components are used under permissive terms that require preserving the
  copyright notice and permission text, reproduced in the license files above
  and in the application's about box (added in a later phase).
