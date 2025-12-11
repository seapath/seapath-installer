#!/bin/bash -e

export DESTDIR=/tmp/sysroot-seapath-installer
export BUILDDIR=./build
export CMAKE_ARGS="-DWITH_QT6=OFF"
export VERSION="1.2.1"
./ci/build.sh
mkdir -p seapath-installer_${VERSION}_all/etc/calamares
mkdir -p seapath-installer_${VERSION}_all/usr/share/calamares/modules
cp settings.conf seapath-installer_${VERSION}_all/etc/calamares
cp partition.conf seapath-installer_${VERSION}_all/usr/share/calamares/modules
cp build/src/modules/finishedq/finishedq.conf seapath-installer_${VERSION}_all/usr/share/calamares/modules
cp -r ./debian/* seapath-installer_${VERSION}_all/
cp -r $DESTDIR/* seapath-installer_${VERSION}_all/
dpkg-deb --build seapath-installer_${VERSION}_all
