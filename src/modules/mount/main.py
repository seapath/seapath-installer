#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
#   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
#   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
#   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
#   SPDX-FileCopyrightText: 2019-2020 Collabora Ltd
#   SPDX-License-Identifier: GPL-3.0-or-later
#
#   Calamares is Free Software: see the License-Identifier above.
#

import tempfile
import subprocess

import libcalamares

import gettext

_ = gettext.translation(
    "calamares-python",
    localedir=libcalamares.utils.gettext_path(),
    languages=libcalamares.utils.gettext_languages(),
    fallback=True,
).gettext


def pretty_name():
    return _("Mounting partitions.")


def copy_files(source, destination):
    try:
        subprocess.run(["cp", "-r", source, destination], check=True)
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(
            f"Failed to copy files from {source} to {destination}"
        )


def append_to_file(source, destination):
    with open(source, "r") as src, open(destination, "a") as dest:
        dest.write(src.read())


def mount_partitions(partition, root_mount_point):
    try:
        subprocess.run(["mount", partition, root_mount_point], check=True)
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to mount partition: {partition}")


def get_seapath_flavor(image_name):
    if "debian" in image_name.lower():
        libcalamares.globalstorage.insert("seapath_flavor", "debian")
        return "debian"
    else:
        libcalamares.globalstorage.insert("seapath_flavor", "yocto")
    return "yocto"


def get_rootfs_partition(device_name):
    try:
        result = subprocess.run(
            ["lsblk", "--list", "--noheadings", "--output", "NAME", device_name],
            capture_output=True,
            text=True,
            check=True,
        )
        partitions = ["/dev/" + word for word in result.stdout.strip().split("\n")]
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to list partitions of {device_name}.")

    selected_image_name = libcalamares.globalstorage.value("imageselection.selected")[0]
    if get_seapath_flavor(selected_image_name) == "debian":
        rootfs_partition = partitions[2]
    else:
        rootfs_partition = partitions[3]
    return rootfs_partition


def run():
    target_disk = libcalamares.globalstorage.value("selectedDisk")
    sshkey = libcalamares.globalstorage.value("sshkeyselection.selectedKeys")
    libcalamares.utils.debug("sshkeyselection: selected keys: {!s}".format(sshkey))
    libcalamares.utils.debug("target disk is {!s}".format(target_disk))
    root_mount_point = tempfile.mkdtemp(prefix="calamares-root-")

    rootfs_partition = get_rootfs_partition(target_disk)

    mount_partitions(rootfs_partition, root_mount_point)

    for key in sshkey:
        append_to_file(key, root_mount_point + "/home/admin/.ssh/authorized_keys")
