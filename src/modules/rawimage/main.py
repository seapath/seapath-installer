#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
#   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
#   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
#   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
#   SPDX-License-Identifier: GPL-3.0-or-later
#
#   Calamares is Free Software: see the License-Identifier above.
#

import subprocess
import re
import gettext
import libcalamares

# Prefer the Calamares process helpers over subprocess for progress updates
from libcalamares.utils import (
    host_env_process_output,
)  # or target_env_process_output if needed

output_lines = []

_ = gettext.translation(
    "calamares-python",
    localedir=libcalamares.utils.gettext_path(),
    languages=libcalamares.utils.gettext_languages(),
    fallback=True,
).gettext


def pretty_name():
    return _("SEAPATH is installing...")


status = _("SEAPATH python step {}").format(0)


def pretty_status_message():
    return status

def remove_volume_group(mount_point):

    try:
        vg = subprocess.run(
            ["/usr/sbin/vgdisplay"], check=True, capture_output=True, text=True
        )
    except subprocess.CalledProcessError:
        # Volume group does not exist; nothing to do
        libcalamares.utils.error(f"Failed to list volume group on: {mount_point}")
        raise

    if(vg.stdout != ""):
        try:
            subprocess.run(
                ["/usr/sbin/vgremove", "-y", "vg2"], check=True
            )
        except subprocess.CalledProcessError:
            libcalamares.utils.warning(f"Failed to disable volume group on: {mount_point}")
            raise


def run():
    """Raw image copy module"""
    gs = libcalamares.globalstorage
    image = gs.value("imageselection.selectedFiles")[0]
    seapath_flavor = libcalamares.globalstorage.value("seapathFlavor")

    libcalamares.utils.debug("SEAPATH flavor selected: {}".format(seapath_flavor))
    libcalamares.utils.debug(f"Selected image: {image}")

    target_device = gs.value("selectedDisk")

    if target_device:
        libcalamares.utils.debug(
            f"Selected boot device (from partition module): {target_device}"
        )

    def output_cb(line: str):
        line = line.rstrip("\n")
        if not line:
            return
        m = re.search(r"\((\d+)%\)", line)
        output_lines.append(line)

        if m:
            try:
                progression = int(m.group(1))
                libcalamares.job.setprogress(progression / 100.0)
            except ValueError:
                libcalamares.utils.debug(
                    f"Could not parse progression from line: {line}"
                )

    libcalamares.utils.debug("Removing LVM volume groups on target device")
    remove_volume_group(target_device)

    try:
        # Run the script on the host; switch to target_env_process_output(),
        # if it must run in target chroot.
        bmaptool_cmd = ["bmaptool", "--debug", "copy"]
        if gs.value("seapathFlavor") == "debian":
            bmaptool_cmd.append("--nobmap")

        bmaptool_cmd.extend([image, target_device])
        host_env_process_output(
            bmaptool_cmd,
            output_cb,  # callback for streaming output
        )
        rc = 0
    except subprocess.CalledProcessError as e:
        rc = e.returncode
        libcalamares.utils.warning(f"Script failed rc={rc}")
        libcalamares.utils.debug("\n".join(output_lines))

    if rc != 0:
        return (
            "SEAPATH installation failed",
            f"Cannot copy the SEAPATH image on the selected device, exit code {rc}",
        )

    return None
