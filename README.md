# hungryeditor

[![ci](https://github.com/dagike/hungryeditor/actions/workflows/ci.yml/badge.svg)](https://github.com/dagike/hungryeditor/actions/workflows/ci.yml)

A fast, native Markdown editor for people who live in the terminal but want a
real editor for prose — the editing feel of Sublime and Notepad++, the live
preview of Typora, and none of the browser weight.

- **Native.** C++20 + Qt 6. Cold start under a quarter second, small memory
  footprint. Not an Electron app, not an IDE.
- **Real syntax highlighting.** Fenced code blocks are highlighted with the
  actual language grammar — a `rust` block inside your `.md` looks like Rust,
  not grey monospace.
- **Live preview.** Side-by-side rendered view with synced scrolling, mermaid
  diagrams and math.
- **Cross-platform.** Linux and Windows, both first-class.

## Status

Early development. See the build instructions below.

## Building

Requirements:

- CMake ≥ 3.24 and Ninja
- A C++20 compiler (GCC 12+, Clang 16+, or MSVC 19.3+)
- Qt 6.4+ — `Widgets` and `Core5Compat` now; `WebEngineWidgets` and
  `WebChannel` from Phase 3

On Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build build-essential \
  qt6-base-dev qt6-5compat-dev qt6-webengine-dev qt6-webchannel-dev
```

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/linux-debug/bin/hungryeditor
```

On Windows use the `windows-release` preset from a Developer prompt.

## License

MIT — see `LICENSE`. Qt is used under LGPLv3 (dynamic linking); third-party
components retain their own licenses — see `THIRD_PARTY.md`.
