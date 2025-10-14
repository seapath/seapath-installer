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

import os
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

SSH_KEYSDIR_AT_HOME = "admin/.ssh"


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
    """
    Append the contents of the source file to the destination file. If the
    destination file does not exist create it.
    """
    if not os.path.exists(source):
        raise FileNotFoundError(f"The source file '{source}' does not exist.")

    if os.path.exists(destination):
        write_mode = "a"
    else:
        write_mode = "w"

    try:
        with open(source, "r") as src, open(destination, write_mode) as dest:
            dest.write(src.read())
    except IOError as e:
        libcalamares.utils.warning(f"An I/O error occured: {e}")


def mount_partition(partition, mount_point):
    try:
        subprocess.run(["mount", partition, mount_point], check=True)
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to mount partition: {partition}")


def mount_overlay(lowerdir, upperdir, workdir, mount_point, options=None):
    """
    Creates upperdir, workdir and the overlayfs using those.
    """
    try:
        os.makedirs(upperdir, exist_ok=True)
        os.makedirs(workdir, exist_ok=True)
    except OSError as error:
        libcalamares.utils.warning(
            f"Failed to create directories for overlay with error {error} "
        )

    mount_options = f"lowerdir={lowerdir},upperdir={upperdir},workdir={workdir}"
    if options:
        mount_options = f"{options},{mount_options}"

    mount_cmd = [
        "mount",
        "-t",
        "overlay",
        "overlay",
        "-o",
        mount_options,
        mount_point,
    ]
    try:
        subprocess.run(mount_cmd, check=True)
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(
            f"Failed to mount overlayfs using the command for {lowerdir}"
        )


def get_partitions(device_name, seapath_flavor):
    """
    This processes the output of lsblk to get rootfs and persistent partitions.
    The output of lsblk will be like:
    nvme0n1
    nvme0n1p1
    nvme0n1p2
    nvme0n1p3
    nvme0n1p4
    nvme0n1p5
    nvme0n1p6
    If the flavor is debian, roofs is the second one. If the flavor is seapath
    rootfs is third and the persistentfs is sixth.

    Returns rootfs and persistent partitions name as string.
    """
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

    persistent_partition = ""
    if seapath_flavor == "debian":
        rootfs_partition = partitions[2]
    else:
        rootfs_partition = partitions[3]
        persistent_partition = partitions[6]
    return rootfs_partition, persistent_partition


def run():
    target_disk = libcalamares.globalstorage.value("selectedDisk")
    sshkey = libcalamares.globalstorage.value("sshkeyselection.selectedKeys")
    libcalamares.utils.debug("sshkeyselection: selected keys: {!s}".format(sshkey))
    libcalamares.utils.debug("target disk is {!s}".format(target_disk))

    home_mount_point = tempfile.mkdtemp(prefix="calamares-home-")
    etc_mount_point = tempfile.mkdtemp(prefix="calamares-etc-")
    libcalamares.globalstorage.insert("homeMountPoint", home_mount_point)
    libcalamares.globalstorage.insert("etcMountPoint", etc_mount_point)

    rootfs0_mount_point = "/mnt/rootfs0"
    libcalamares.globalstorage.insert("rootMountPoint", rootfs0_mount_point)
    persistent_mount_point = "/mnt/persistent"
    os.makedirs(rootfs0_mount_point, exist_ok=True)
    os.makedirs(persistent_mount_point, exist_ok=True)

    seapath_flavor = libcalamares.globalstorage.value("seapathFlavor")
    rootfs_partition, persistent_partition = get_partitions(target_disk, seapath_flavor)

    mount_partition(rootfs_partition, rootfs0_mount_point)

    # Mount overlayfs
    if seapath_flavor == "yocto":
        mount_partition(persistent_partition, persistent_mount_point)
        mount_overlay(
            f"{rootfs0_mount_point}/home",
            f"{persistent_mount_point}/home",
            f"{persistent_mount_point}/.home-work",
            home_mount_point,
        )
        mount_overlay(
            f"{rootfs0_mount_point}/etc",
            f"{persistent_mount_point}/etc",
            f"{persistent_mount_point}/.etc-work",
            etc_mount_point,
            options="rw,relatime",
        )
        sshdir = os.path.join(home_mount_point, SSH_KEYSDIR_AT_HOME)
        os.makedirs(sshdir, exist_ok=True)
    else:
        sshdir = os.path.join(rootfs0_mount_point, "home", SSH_KEYSDIR_AT_HOME)

    for key in sshkey:
        append_to_file(key, os.path.join(sshdir, "authorized_keys"))
