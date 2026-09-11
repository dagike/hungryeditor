# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- SQL grammar for fenced code blocks (plus `postgresql`/`psql`/`mysql`
  aliases).

### Fixed

- Highlight-query predicates (`#match?`, `#eq?`, `#any-of?`, and their
  negations) are now evaluated instead of ignored, so captures gated on them
  no longer over-fire — for example, a lowercase Python identifier no longer
  gets styled as a constant just because a later, unconditional-looking rule
  in the query file happened to come last.

## [0.1.0] - 2026-09-11

First tagged release.

### Added

- **Editor.** A Scintilla-backed editing surface with multi-cursor and
  select-next-occurrence, rectangular column selection, find/replace with
  regex, find in files, a fuzzy command palette, go-to-anything file/buffer
  search, line move/duplicate/delete/join, toggle comment, bracket matching
  and auto-indent.
- **Syntax highlighting.** Fenced code blocks highlighted with real
  tree-sitter grammars (Bash, C, C++, C#, CSS, Go, HTML, Java,
  JavaScript/JSX, JSON, PHP, Python, Ruby, Rust, TOML, TypeScript/TSX, YAML)
  via container lexing, with markdown-inline injections.
- **Markdown authoring.** Bold/italic/strikethrough/code/link shortcuts,
  heading cycling, smart paste for URLs and clipboard images, GFM tables with
  tab navigation and auto-alignment, task lists, footnotes, autolinks, YAML
  front-matter folding, an outline panel tracking the caret, and clickable
  task checkboxes that write back to the source.
- **Live preview.** A QtWebEngine-backed preview pane with synced
  bidirectional scrolling, themed syntax highlighting in fenced code blocks,
  bundled mermaid diagrams and KaTeX math (fully offline), lazy rendering on
  scroll, and a strict CSP blocking external resources.
- **Files & workspace.** Tabs with dirty markers and reordering, drag and
  drop, external-change detection with reload prompts, crash-safe autosave
  drafts, session restore (tabs, cursors, scroll, geometry, panel layout),
  a recent-files menu with pinning, a filtered file-tree sidebar, MRU tab
  switching, and caret history navigation.
- **Export & theming.** Standalone HTML export with inlined assets, print
  and PDF export, copy as rich text, four built-in themes (light, dark,
  high-contrast, sepia) sharing one token map between the editor and the
  preview, user JSON theme files with custom preview CSS, and a preferences
  dialog for fonts, tab width and word wrap.
- **Diagnostics.** An opt-in local crash reporter (off by default) that
  writes a report with a backtrace on a real crash, on both Linux and
  Windows.
- **Packaging.** `.deb` and `.rpm` for Linux, `.msi` and a portable `.zip`
  for Windows, an AppImage build script and a Flatpak manifest, and a
  tag-triggered release workflow that builds and publishes all of the above.

[Unreleased]: https://github.com/dagike/hungryeditor/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/dagike/hungryeditor/releases/tag/v0.1.0
