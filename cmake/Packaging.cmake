# install() rules and CPack configuration for distributable packages.
# Included once from the top-level CMakeLists.txt after the `hungryeditor`
# target exists. Grows across Phase 10 (deb/rpm here; AppImage/Flatpak,
# Windows MSI/zip and release automation are later commits in the same
# phase) rather than being one big packaging pass.

include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    install(TARGETS hungryeditor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/hungryeditor.desktop"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

    # A single scalable SVG in the hicolor theme is enough for every modern
    # Linux desktop environment — no need to hand-generate fixed PNG sizes.
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/hungryeditor.svg"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")

    # text/markdown is already a standard freedesktop.org shared-mime-info
    # type (registered for .md/.markdown/.mkd/.mdown) on any desktop new
    # enough to matter here, so there is no custom MIME XML to install —
    # the .desktop file's MimeType= entry above is enough to make
    # hungryeditor an available (and, once picked, default) handler for it.

    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_PACKAGE_NAME "hungryeditor")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "hungryeditor")
    set(CPACK_PACKAGE_CONTACT "dagike <dagike@users.noreply.github.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    # No CPACK_RESOURCE_FILE_LICENSE / *_PACKAGE_LICENSE yet: the project's
    # own license is still "to be finalized" (see README.md) — that and the
    # LICENSE file it depends on are 10.5's job, not this commit's to guess.

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
endif()
