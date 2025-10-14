#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
#   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
#   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: GPL-3.0-or-later
#
#   Calamares is Free Software: see the License-Identifier above.
#

import subprocess
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

gs = libcalamares.globalstorage
image = gs.value("imageselection.selectedFiles")[0]
libcalamares.utils.debug(f"Selected image: {image}")


target_device = gs.value("selectedDisk")


def pretty_name():
    return _("SEAPATH is installing...")


status = _("UEFI boot step {}").format(0)


def pretty_status_message():
    return status


def get_boot_entry(slot):
    entry = ""
    try:
        output = subprocess.check_output(["efibootmgr"], text=True)
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(f"Failed to list boot entries: {e}")
        raise

    for line in output.splitlines():
        if f"SEAPATH slot {slot}" in line:
            first_field = line.split()[0]  # Boot0003*
            entry = first_field.replace("Boot", "").replace("*", "")
            break

    return entry


def delete_boot_entries(slot):
    entry_num = get_boot_entry(slot)

    try:
        subprocess.run(["efibootmgr", "-q", "-b", entry_num, "-B"])
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(f"Failed to delete boot entry {entry_num}: {e}")
        raise


def create_boot_entries(slot):
    try:
        subprocess.run(
            [
                "efibootmgr",
                "-q",
                "-c",
                "-d",
                target_device,
                "-p",
                str(slot + 1),
                "-L",
                f"SEAPATH slot {slot}",
                "-l",
                "/EFI/BOOT/bootx64.efi",
            ],
            check=True,
        )
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(f"Failed to create boot entry for slot {slot}: {e}")
        raise


def reorder_boot_entries():
    active_boot = get_boot_entry(0)
    passive_boot = get_boot_entry(1)
    try:
        subprocess.run(["efibootmgr", "-q", "-b", passive_boot, "-A"])
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(
            f"Failed to disable SEAPATH boot entry {passive_boot}: {e}"
        )
        raise

    boot_order = None
    out = subprocess.check_output(["efibootmgr"], text=True)
    for line in out.splitlines():
        if line.startswith("BootOrder:"):
            parts = line.split()
            if len(parts) >= 2:
                boot_order = parts[1].strip()
            break

    if not boot_order:
        remaining = []
    else:
        parts = [p.strip() for p in boot_order.split(",") if p.strip()]
        remaining = [p for p in parts if p not in {active_boot, passive_boot}]
    ordered = []
    if active_boot:
        ordered.append(active_boot)
    if passive_boot:
        ordered.append(passive_boot)
    ordered.extend(remaining)

    boot_order = ",".join(ordered)

    try:
        subprocess.run(["efibootmgr", "-q", "-o", boot_order], check=True)
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(f"Failed to reorder boot entries: {e}")
        raise

    try:
        subprocess.run(["efibootmgr", "--bootnext", active_boot], check=True)
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(f"Failed to set boot entry {active_boot} as next: {e}")
        raise


def is_efivars_mounted():
    try:
        with open("/proc/mounts") as f:
            return any("/sys/firmware/efi/efivars" in line for line in f)
    except Exception as e:
        libcalamares.utils.error(f"Failed to check if efivars is mounted: {e}")
        raise


def mount_efivars():
    try:
        subprocess.run(
            ["mount", "-t", "efivarfs", "efivarfs", "/sys/firmware/efi/efivars"],
            check=True,
        )
    except subprocess.CalledProcessError as e:
        libcalamares.utils.error(f"Failed to mount efivars: {e}")
        raise


def run():
    if not is_efivars_mounted():
        mount_efivars()

    delete_boot_entries(0)
    delete_boot_entries(1)

    create_boot_entries(0)
    create_boot_entries(1)

    reorder_boot_entries()

    return None
