// Proves the vendored static libraries link and expose usable symbols.
// Runs headless via the Qt "offscreen" platform plugin.

#include <cstdio>

#include <QApplication>

#include <ILexer.h>
#include <Lexilla.h>

#include <ScintillaEditBase.h>
#include <ScintillaMessages.h>
#include <ScintillaTypes.h>

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // --- Lexilla: catalogue is populated and known lexers resolve --------
    const int count = GetLexerCount();
    if (count < 100) {
        std::fprintf(stderr, "lexilla: expected 100+ lexers, got %d\n", count);
        return 1;
    }
    for (const char* name : {"cpp", "python", "markdown", "rust", "bash"}) {
        Scintilla::ILexer5* lexer = CreateLexer(name);
        if (!lexer) {
            std::fprintf(stderr, "lexilla: CreateLexer(\"%s\") returned null\n", name);
            return 1;
        }
        lexer->Release();
    }

    // --- Scintilla: construct the widget and round-trip text through it --
    ScintillaEditBase editor;
    editor.send(static_cast<unsigned int>(Scintilla::Message::AddText), 5,
                reinterpret_cast<Scintilla::sptr_t>("hello"));
    const auto length = editor.send(static_cast<unsigned int>(Scintilla::Message::GetLength));
    if (length != 5) {
        std::fprintf(stderr, "scintilla: expected length 5, got %lld\n",
                     static_cast<long long>(length));
        return 1;
    }

    std::printf("ok: %d lexers; scintilla buffer length %lld\n", count,
                static_cast<long long>(length));
    return 0;
}
