#!/bin/bash -e

export DESTDIR=/tmp/sysroot-seapath-installer
export BUILDDIR=./build
export CMAKE_ARGS="-DWITH_QT6=OFF"

./ci/build.sh
mkdir -p seapath-installer_1.0_all
cp -r ./debian/* seapath-installer_1.0_all/
cp -r $DESTDIR/* seapath-installer_1.0_all/
dpkg-deb --build seapath-installer_1.0_all
