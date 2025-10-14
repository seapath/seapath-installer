#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# === This file is part of Seapath installer ===
#
#   SPDX-FileCopyrightText: 2014 Philip Müller <philm@manjaro.org>
#   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
#   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
#   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
#   SPDX-FileCopyrightText: 2021 Anke boersma <demm@koasx.us>
#   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
#   SPDX-License-Identifier: GPL-3.0-or-later
#

import os
import libcalamares

import gettext

_ = gettext.translation(
    "calamares-python",
    localedir=libcalamares.utils.gettext_path(),
    languages=libcalamares.utils.gettext_languages(),
    fallback=True,
).gettext

NETWORKD_CONFIG_FILE_NAME = "01-installer.network"
NETWORKD_CONFIG_DIR_AT_ETC = "systemd/network"


def pretty_name():
    return _("Saving network configuration.")


def get_live_user():
    """
    Gets the "live user" login. This might be "live", or "nitrux",
    or something similar: it is the login name used *right now*,
    and network configurations saved for that user, should be applied
    also for the installed user (which probably has a different name).
    """
    # getlogin() is a thin-wrapper, and depends on getlogin(3),
    # which reads utmp -- and utmp isn't always set up right.
    try:
        return os.getlogin()
    except OSError:
        pass
    # getpass will return the **current** user, which is generally root.
    # That isn't very useful, because the network settings have been
    # made outside of Calamares-running-as-root, as a different user.
    #
    # If Calamares is running as non-root, though, this is fine.
    import getpass

    name = getpass.getuser()
    if name != "root":
        return name

    # TODO: other mechanisms, e.g. guessing that "live" is the name
    # TODO: support a what-is-the-live-user setting
    return None


def complete_ipv4_address(ipaddress, mask):
    """
    Returns complete ipv4 address in the format ipaddress/mask.
    """
    return ipaddress + "/" + mask


def create_network_file(
    systemd_networkd_config_file_path,
    network_interface_name,
    ip_address_with_mask,
    gateway,
    use_dhcp=False,
):
    """
    Create the systemd-networkd configuration file with network interface name,
    IPv4 address with the subnet mask value as well as the gateway address at
    systemd_networkd_config_file_path of the live user with NETWORKD_CONFIG_FILE_NAME.
    Format of the content:
        [Match]
        Name=enp3s0

        [Network]
        Address=1.1.1.1/24
        Gateway=1.1.1.1
    If the using dhcp, the file will have corresponding content:
        [Match]
        Name=ensp3s0

        [Network]
        DHCP=ipv4
    """
    content = (
        f"[Match]\nName={network_interface_name}\n\n"
        + f"[Network]\nAddress={ip_address_with_mask}\nGateway={gateway}"
    )

    if use_dhcp:
        content = f"[Match]\nName={network_interface_name}\n\n" + "[Network]\nDHCP=ipv4"

    try:
        with open(systemd_networkd_config_file_path, "w") as file:
            file.write(content)
    except (PermissionError, OSError):
        libcalamares.utils.debug(
            "Error writing to file {!s}.".format(systemd_networkd_config_file_path)
        )


def run():
    """
    Setup network configuration
    """
    if libcalamares.globalstorage.value("seapathFlavor") == "debian":
        root_mount_point = libcalamares.globalstorage.value("rootMountPoint")
        etc_mount_point = root_mount_point + "/etc"
    else:
        etc_mount_point = libcalamares.globalstorage.value("etcMountPoint")

    if etc_mount_point is None:
        libcalamares.utils.warning(
            "etcMountPoint is empty, {!s}".format(etc_mount_point)
        )
        return (
            _("Configuration Error"),
            _("No root mount point is given for <pre>{!s}</pre> to use.").format(
                "networkcfg"
            ),
        )

    use_dhcp = libcalamares.globalstorage.value("useDhcp")
    network_interface_name = libcalamares.globalstorage.value("networkInterface")
    if not use_dhcp:
        ip_address = libcalamares.globalstorage.value("ipAddress")
        mask = libcalamares.globalstorage.value("mask")
        gateway = libcalamares.globalstorage.value("gateway")
        ip_address_with_mask = complete_ipv4_address(ip_address, mask)
    else:
        ip_address_with_mask = ""
        gateway = ""

    network_cfg_path = os.path.join(etc_mount_point, NETWORKD_CONFIG_DIR_AT_ETC)
    if not os.path.exists(network_cfg_path):
        os.makedirs(etc_mount_point, NETWORKD_CONFIG_DIR_AT_ETC, exist_ok=True)

    network_cfg = os.path.join(network_cfg_path, NETWORKD_CONFIG_FILE_NAME)
    create_network_file(
        network_cfg,
        network_interface_name,
        ip_address_with_mask,
        gateway,
        use_dhcp,
    )
