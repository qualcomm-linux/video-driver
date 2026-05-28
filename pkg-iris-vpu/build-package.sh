#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
set -e

# Determine script location and project structure
SCRIPT_PATH="$(realpath "$0")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

# Check if we're running from pkg-iris-vpu directory or video-driver root
if [[ "$SCRIPT_DIR" == */pkg-iris-vpu ]]; then
    # Running from pkg-iris-vpu directory
    DKMS_DEBIAN_DIR="$SCRIPT_DIR"
    VIDEO_DRIVER_ROOT="$(dirname "$DKMS_DEBIAN_DIR")"
else
    # Running from video-driver root directory
    VIDEO_DRIVER_ROOT="$SCRIPT_DIR"
    DKMS_DEBIAN_DIR="$VIDEO_DRIVER_ROOT/pkg-iris-vpu"
fi

BUILD_OUTPUT="$DKMS_DEBIAN_DIR/build"

echo "Building DKMS package for iris-vpu..."
echo "Script location: $SCRIPT_DIR"
echo "pkg-iris-vpu directory: $DKMS_DEBIAN_DIR"
echo "Video driver root: $VIDEO_DRIVER_ROOT"

# Resolve package version from git tag (single source of truth)
cd "$VIDEO_DRIVER_ROOT"
GIT_TAG=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
if [ -z "$GIT_TAG" ]; then
    # Fallback: use PACKAGE_VERSION from dkms.conf
    GIT_TAG=$(grep 'PACKAGE_VERSION=' "$DKMS_DEBIAN_DIR/dkms.conf" | cut -d'"' -f2)
    echo "No git tag found, using dkms.conf version: $GIT_TAG"
else
    echo "Package version from git tag: $GIT_TAG"
fi

# Create output directory
mkdir -p "$BUILD_OUTPUT"

# Execute build in video-driver root directory
cd "$VIDEO_DRIVER_ROOT"

echo "Preparing build environment..."

# Clean up any previous build artifacts first
echo "Cleaning up any previous build artifacts..."
rm -rf debian dkms.conf scripts 2>/dev/null || true

# Check if we need to copy files (avoid copying to same location)
if [ "$PWD" != "$DKMS_DEBIAN_DIR" ]; then
    # Temporarily copy pkg-iris-vpu files to root directory (cleanup after build)
    echo "Copying debian configuration files..."
    cp -r "$DKMS_DEBIAN_DIR/debian" ./
    cp "$DKMS_DEBIAN_DIR/dkms.conf" ./

    # Copy scripts directory to root directory
    echo "Copying build scripts..."
    cp -r "$DKMS_DEBIAN_DIR/scripts" ./
else
    echo "Already in pkg-iris-vpu directory, skipping file copy..."
fi

# Sync version from git tag into dkms.conf and debian/changelog
echo "Syncing version $GIT_TAG into dkms.conf and debian/changelog..."
sed -i "s/PACKAGE_VERSION=\"[^\"]*\"/PACKAGE_VERSION=\"${GIT_TAG}\"/" dkms.conf
sed -i "s/^iris-vpu ([^)]*)/iris-vpu (${GIT_TAG}-1)/" debian/changelog
echo "  dkms.conf PACKAGE_VERSION -> $GIT_TAG"
echo "  debian/changelog version  -> ${GIT_TAG}-1"

# Set script execution permissions
chmod +x scripts/*.sh
chmod +x debian/rules

echo "Building debian package..."
# Build debian package
dpkg-buildpackage -us -uc -b

echo "Moving build artifacts..."
# Move all generated package files (deb, buildinfo, changes, dsc, etc.) to pkg-iris-vpu/build
PARENT_DIR="$(dirname "$VIDEO_DRIVER_ROOT")"
mv "$PARENT_DIR"/iris-vpu_* "$BUILD_OUTPUT/" 2>/dev/null || true
mv "$PARENT_DIR"/iris-vpu-dkms_* "$BUILD_OUTPUT/" 2>/dev/null || true

echo "Cleaning up temporary files..."
# Clean up temporary files
rm -rf debian dkms.conf scripts
rm -f "$PARENT_DIR"/iris-vpu_* "$PARENT_DIR"/iris-vpu-dkms_* 2>/dev/null || true

echo "Build completed successfully!"
echo "Package available in: $BUILD_OUTPUT"
ls -la "$BUILD_OUTPUT"