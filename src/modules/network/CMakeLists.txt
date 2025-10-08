# === This file is part of Seapath installer ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED Core DBus Network)

include_directories(${PROJECT_BINARY_DIR}/src/libcalamaresui)

set(_network_src
    # Configuration
    Config.cpp
)

# This part of the code is shared with the networkq module
add_library(network_internal
    OBJECT
        ${_network_src}
)
target_link_libraries(network_internal
    PRIVATE
        ${Calamares_LIBRARIES}
        ${qtname}::Core
        ${qtname}::Gui
        ${qtname}::Widgets
)
target_compile_definitions(network_internal PUBLIC PLUGINDLLEXPORT_PRO)
target_compile_options(network_internal PUBLIC -fPIC)
calamares_automoc(network_internal)

calamares_add_plugin(network
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        NetworkViewStep.cpp
        NetworkPage.cpp
        NetworkInterfaceList.cpp
    UI
        NetworkPage.ui
    LINK_PRIVATE_LIBRARIES
        network_internal
    SHARED_LIB
)
