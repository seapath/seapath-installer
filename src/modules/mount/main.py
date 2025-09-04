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
import os
import re

import libcalamares

import gettext

_ = gettext.translation("calamares-python",
                        localedir=libcalamares.utils.gettext_path(),
                        languages=libcalamares.utils.gettext_languages(),
                        fallback=True).gettext

def pretty_name():
    return _("Mounting partitions.")

def copy_files(source, destination):
    try:
        subprocess.run(["cp", "-r", source, destination], check=True)
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to copy files from {source} to {destination}")

def append_to_file(source, destination):
    with open(source, 'r') as src, open(destination, 'a') as dest:
        dest.write(src.read())


def mount_partitions(partition, root_mount_point):
    try:
        subprocess.run(["mount", partition, root_mount_point], check=True)
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to mount partition: {partition}")

def run():
    target_disk = libcalamares.globalstorage.value("selectedDisk")
    sshkey = libcalamares.globalstorage.value("sshkeyselection.selectedKeys")
    libcalamares.utils.debug("sshkeyselection: selected keys: {!s}".format(sshkey))
    libcalamares.utils.debug("target disk is {!s}".format(target_disk))
    root_mount_point = tempfile.mkdtemp(prefix="calamares-root-")

    mount_partitions(f"{target_disk}3", root_mount_point)

    for key in sshkey:
        append_to_file(key, root_mount_point + "/home/admin/.ssh/authorized_keys")
