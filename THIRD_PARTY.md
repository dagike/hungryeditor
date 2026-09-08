# Third-party components

All vendored sources live under `third_party/` and are kept as close to
upstream as practical — only build scripts and IDE project files were removed.

| Component | Version | Upstream | License |
|-----------|---------|----------|---------|
| Scintilla | 5.5.5 (`555`) | https://www.scintilla.org/ | HPND-style — `third_party/scintilla/License.txt` |
| Lexilla | 5.4.0 (`540`) | https://www.scintilla.org/Lexilla.html | HPND-style — `third_party/lexilla/License.txt` |

## Notes

- **Scintilla** is built with its Qt platform layer
  (`qt/ScintillaEditBase/`) and linked statically. It requires
  `Qt6::Core5Compat` for `QTextCodec` (legacy codepage handling).
- **Lexilla** provides the stock lexers used only on the large-file fallback
  path (files above the tree-sitter size limit). The full lexer catalogue is
  compiled; trimming it is a later size-optimization decision.
- Both are used under permissive terms that require preserving the copyright
  notice and permission text, reproduced in the license files above and in
  the application's about box (added in a later phase).
