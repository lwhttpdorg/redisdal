#!/bin/sh

# Usage: ./build-deb.sh [clean]
# Default: build packages. If first arg is "clean", only run clean.

BUILD_OPTIONS="nocheck terse"
DEB_BUILD_OPTIONS=${BUILD_OPTIONS} debian/rules clean

if [ "$1" = "clean" ]; then
	# delete deb, buildinfo, changes
	find .. -name "*.deb" -delete
	find .. -name "*.buildinfo" -delete
	find .. -name "*.changes" -delete
	exit 0
fi

DEB_BUILD_OPTIONS=${BUILD_OPTIONS} dpkg-buildpackage -us -uc -b -j"$(nproc)"
