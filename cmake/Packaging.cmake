# install() rules and CPack configuration for distributable packages.
# Included once from the top-level CMakeLists.txt after the `hungryeditor`
# target exists. Covers deb/rpm and Windows msi/zip; AppImage/Flatpak are
# separate scripts/manifests, not CPack; release automation lives in
# .github/workflows/release.yml.

include(GNUInstallDirs)

# --- Shared package identity -------------------------------------------------
set(CPACK_PACKAGE_NAME "hungryeditor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "hungryeditor")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

if(UNIX AND NOT APPLE)
    install(TARGETS hungryeditor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/hungryeditor.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

    # A single scalable SVG in the hicolor theme is enough for every modern
    # Linux desktop environment — no need to hand-generate fixed PNG sizes.
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/hungryeditor.svg"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")

    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/hungryeditor")

    # text/markdown is already a standard freedesktop.org shared-mime-info
    # type (registered for .md/.markdown/.mkd/.mdown) on any desktop new
    # enough to matter here, so there is no custom MIME XML to install —
    # the .desktop file's MimeType= entry above is enough to make
    # hungryeditor an available (and, once picked, default) handler for it.

    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_PACKAGE_CONTACT "dagike <dagike@users.noreply.github.com>")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

    # Runtime library names only (no -dev packages) — matches what
    # `ldd build/*/bin/hungryeditor` actually reports linked at runtime.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6, libqt6gui6, libqt6core6, libqt6core5compat6, \
libqt6webenginewidgets6, libqt6webenginecore6, libqt6webchannel6, \
libqt6printsupport6, libqt6network6, libqt6positioning6, libqt6opengl6")
    set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dagike/hungryeditor")

    # Fedora/openSUSE spell these differently than Debian/Ubuntu; this list
    # targets Fedora's qt6-qt* naming and is unverified locally (no
    # rpmbuild in this environment — CPack's RPM generator itself is
    # exercised, just not the exact dependency names against a live repo).
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase-gui, qt6-qt5compat, qt6-qtwebengine, qt6-qtwebchannel")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/dagike/hungryeditor")

    include(CPack)
elseif(WIN32)
    # Flat layout (no bin/ subfolder): both the MSI's install directory and
    # the portable ZIP's extracted folder should just be "the app", with the
    # exe and its bundled Qt DLLs side by side.
    install(TARGETS hungryeditor RUNTIME DESTINATION .)

    # Qt ships no equivalent of Linux's dynamic linker search path here, so
    # the exe needs its Qt DLLs (and platform/imageformats/etc. plugins)
    # copied in next to it — windeployqt is Qt's own tool for exactly that.
    # Entirely unverified locally: this repo has no Windows environment, so
    # neither the windeployqt discovery below nor the packages it feeds have
    # ever actually run; CI's windows-latest job is the first real test.
    #
    # Qt6::qmake is defined by find_package(Qt6 ...) in src/CMakeLists.txt,
    # but that is a child directory scope — re-finding it here (a cheap,
    # cached no-op) guarantees the target exists in this scope too, rather
    # than relying on Qt's own CMake config happening to mark it GLOBAL.
    find_package(Qt6 REQUIRED COMPONENTS Core)
    get_target_property(_qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)
    if(_qt6_qmake_location)
        get_filename_component(_qt6_bin_dir "${_qt6_qmake_location}" DIRECTORY)
    endif()
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt6_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                --no-compiler-runtime
                --no-translations
                \"\${CMAKE_INSTALL_PREFIX}/hungryeditor.exe\")
        ")
    else()
        message(WARNING
            "windeployqt not found: the installed hungryeditor.exe will be "
            "missing its Qt DLLs. Packaging will still produce an archive, "
            "just not a runnable one.")
    endif()

    set(CPACK_GENERATOR "WIX;ZIP")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "hungryeditor")
    set(CPACK_PACKAGE_EXECUTABLES "hungryeditor;hungryeditor")
    # Fixed and never to be regenerated: WIX/MSI uses this to recognise a new
    # version as an upgrade of the same product rather than a separate,
    # side-by-side install. Generated once with `python3 -c "import uuid;
    # print(uuid.uuid4())"`.
    set(CPACK_WIX_UPGRADE_GUID "A1ECFA45-6525-4652-A30A-EE2AD4312CDC")
    # No CPACK_WIX_PRODUCT_ICON yet: it needs a multi-resolution .ico, and
    # this sandbox has no SVG rasterizer to derive one from
    # resources/icons/hungryeditor.svg (itself only a placeholder — see its
    # own comment). WIX falls back to a generic installer icon until then.

    include(CPack)
endif()
