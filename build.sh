#!/bin/bash -e

export DESTDIR=/tmp/sysroot-seapath-installer
export BUILDDIR=./build
export CMAKE_ARGS="-DWITH_QT6=OFF"

./ci/build.sh
mkdir -p seapath-installer_1.0_all/etc/calamares
mkdir -p seapath-installer_1.0_all/usr/share/calamares/modules
cp settings.conf seapath-installer_1.0_all/etc/calamares
cp partition.conf seapath-installer_1.0_all/usr/share/calamares/modules
cp -r ./debian/* seapath-installer_1.0_all/
cp -r $DESTDIR/* seapath-installer_1.0_all/
dpkg-deb --build seapath-installer_1.0_all
