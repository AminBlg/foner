# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(foner VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.5.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets DBus LinguistTools Test)

# Note: use per-framework find_package (not the aggregate KF6 COMPONENTS),
# which fails to locate some KF6 configs on this system.
find_package(KF6Kirigami ${KF_MIN_VERSION} REQUIRED)
find_package(KF6CoreAddons ${KF_MIN_VERSION} REQUIRED)
find_package(KF6DBusAddons ${KF_MIN_VERSION} REQUIRED)
find_package(KF6I18n ${KF_MIN_VERSION} REQUIRED)

# KContacts ships its own find module name (KF6Contacts), not KF6KContacts.
find_package(KF6Contacts ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Notifications ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Config ${KF_MIN_VERSION} REQUIRED)
# The tray entry speaks the StatusNotifierItem protocol, which every modern tray
# implements; QMenu for its context menu is why Widgets is needed.
find_package(KF6StatusNotifierItem ${KF_MIN_VERSION} REQUIRED)

# main.qml imports org.kde.kitemmodels for KSortFilterProxyModel. Nothing in
# the C++ links it, so the build succeeded on any machine that happened to
# have it installed and the dependency went unrecorded until a container
# without it failed qmllint.
find_package(KF6ItemModels ${KF_MIN_VERSION} REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

# Desktop file + icon (use data-dir based paths; KDE_*_DIR specifics are empty in a bare build)
install(FILES packaging/org.unscale.foner.desktop
        DESTINATION "${KDE_INSTALL_DATADIR}/applications")
install(FILES packaging/icons/org.unscale.foner.svg
        DESTINATION "${KDE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

# AppStream metadata, without which the app never appears in Discover or
# GNOME Software.
install(FILES packaging/org.unscale.foner.metainfo.xml
        DESTINATION "${KDE_INSTALL_DATADIR}/metainfo")

# Declares each notification event, so they are listed and configurable in the
# system notification settings rather than arriving as anonymous popups.
install(FILES packaging/foner.notifyrc
        DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

# Packaging. The same install rules feed every generator, so `cpack -G DEB`,
# `cpack -G RPM` and `cpack -G TGZ` all produce the five installed files.
# Dependencies are derived by dpkg-shlibdeps and rpmbuild from the linked
# binary rather than listed here, where they would be a guess that rots.
set(CPACK_PACKAGE_NAME "foner")
set(CPACK_PACKAGE_VENDOR "AminBlg")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Make and receive phone calls through a paired Bluetooth phone")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/AminBlg/foner")
set(CPACK_PACKAGE_CONTACT "https://github.com/AminBlg/foner/issues")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_GENERATOR "TGZ")

set(CPACK_DEBIAN_PACKAGE_SECTION "comm")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Communications")
# These belong to filesystem packages, not to this one.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications
    /usr/share/icons
    /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps
    /usr/share/metainfo)

include(CPack)