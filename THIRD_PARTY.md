# Third-party components

All vendored sources live under `third_party/` and are kept as close to
upstream as practical — only build scripts and IDE project files were removed.

| Component | Version | Upstream | License |
|-----------|---------|----------|---------|
| Scintilla | 5.5.5 (`555`) | https://www.scintilla.org/ | HPND-style — `third_party/scintilla/License.txt` |
| Lexilla | 5.4.0 (`540`) | https://www.scintilla.org/Lexilla.html | HPND-style — `third_party/lexilla/License.txt` |
| tree-sitter | 0.24.7 | https://github.com/tree-sitter/tree-sitter | MIT — `third_party/tree-sitter/LICENSE` |
| tree-sitter-markdown | 0.4.1 | https://github.com/tree-sitter-grammars/tree-sitter-markdown | MIT — `third_party/tree-sitter-markdown/LICENSE` |
| md4c | 0.5.2 (`release-0.5.2`) | https://github.com/mity/md4c | MIT — `third_party/md4c/LICENSE.md` |
| mermaid | 10.9.1 | https://github.com/mermaid-js/mermaid | MIT — `third_party/mermaid/LICENSE` |
| KaTeX | 0.16.11 | https://github.com/KaTeX/KaTeX | MIT — `third_party/katex/LICENSE` |

### Language grammars (`third_party/grammars/`)

Pre-generated `parser.c` / `scanner.c` plus `queries/highlights.scm`, all MIT.
Each grammar's `LICENSE` is kept alongside its sources.

| Grammar | Version | Upstream |
|---------|---------|----------|
| bash | v0.23.3 | https://github.com/tree-sitter/tree-sitter-bash |
| c | v0.23.4 | https://github.com/tree-sitter/tree-sitter-c |
| cpp | v0.23.4 | https://github.com/tree-sitter/tree-sitter-cpp |
| c-sharp | v0.23.1 | https://github.com/tree-sitter/tree-sitter-c-sharp |
| css | v0.23.2 | https://github.com/tree-sitter/tree-sitter-css |
| go | v0.23.4 | https://github.com/tree-sitter/tree-sitter-go |
| html | v0.23.2 | https://github.com/tree-sitter/tree-sitter-html |
| java | v0.23.5 | https://github.com/tree-sitter/tree-sitter-java |
| javascript | v0.23.1 | https://github.com/tree-sitter/tree-sitter-javascript |
| json | v0.24.8 | https://github.com/tree-sitter/tree-sitter-json |
| php | v0.23.11 | https://github.com/tree-sitter/tree-sitter-php |
| python | v0.23.6 | https://github.com/tree-sitter/tree-sitter-python |
| ruby | v0.23.1 | https://github.com/tree-sitter/tree-sitter-ruby |
| rust | v0.23.2 | https://github.com/tree-sitter/tree-sitter-rust |
| toml | v0.7.0 | https://github.com/tree-sitter-grammars/tree-sitter-toml |
| typescript + tsx | v0.23.2 | https://github.com/tree-sitter/tree-sitter-typescript |
| yaml | v0.7.1 | https://github.com/tree-sitter-grammars/tree-sitter-yaml |

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
  Phase 1 onward.
- **Language grammars** under `third_party/grammars/` back the fenced-code
  registry (`src/highlight/GrammarRegistry`). Only `src/*.c`, the bundled
  `src/tree_sitter/*.h`, and `queries/highlights.scm` are compiled or read;
  a dialect that layers on a base language (C++ on C, TypeScript on
  JavaScript) has the base query concatenated ahead of its own at build time.
  Predicate directives in the queries (`#match?`, `#eq?`) are not yet
  evaluated, so a few captures over-fire slightly.
- **md4c** is compiled from its single `src/md4c.c` (its bundled HTML renderer
  and `entity.c` table are not vendored — `src/markdown/Md4cRenderer` renders
  the parser callbacks directly so it can add `data-src-line` anchors for
  scroll sync). Parses `MD_DIALECT_GITHUB` (tables, task lists, strikethrough,
  autolinks) plus `MD_FLAG_LATEXMATHSPANS` for `$…$` / `$$…$$` math.
- **mermaid** is the pre-built UMD bundle (`dist/mermaid.min.js`), kept as a
  data file — not compiled. It is embedded in the binary as a Qt resource
  (`qrc:/hungryeditor/preview/mermaid.min.js`) and loaded by the preview shell
  page, so ` ```mermaid ` diagrams render with no network access. Nothing links
  against it.
- **KaTeX** ships the same way: the pre-built `katex.min.js` / `katex.min.css`
  from the npm tarball's `dist/`, plus the 20 `fonts/*.woff2` faces the
  stylesheet requests (the `.woff` / `.ttf` fallbacks are dropped — every
  supported browser picks woff2). All embedded at
  `qrc:/hungryeditor/preview/`, so `$…$` / `$$…$$` math renders offline. Not
  compiled, nothing links against it.
- All components are used under permissive terms that require preserving the
  copyright notice and permission text, reproduced in the license files above.
  hungryeditor's own code is MIT-licensed — see `LICENSE`; its Help → About
  box points back to this file for the third-party terms.
