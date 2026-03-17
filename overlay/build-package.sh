#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
set -e

# Determine script location and project structure
SCRIPT_PATH="$(realpath "$0")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

# Check if we're running from overlay directory or video-driver root
if [[ "$SCRIPT_DIR" == */overlay ]]; then
    # Running from overlay directory
    OVERLAY_DIR="$SCRIPT_DIR"
    VIDEO_DRIVER_ROOT="$(dirname "$OVERLAY_DIR")"
else
    # Running from video-driver root directory
    VIDEO_DRIVER_ROOT="$SCRIPT_DIR"
    OVERLAY_DIR="$VIDEO_DRIVER_ROOT/overlay"
fi

BUILD_OUTPUT="$OVERLAY_DIR/build"

echo "Building DKMS package for video-driver..."
echo "Script location: $SCRIPT_DIR"
echo "Overlay directory: $OVERLAY_DIR"
echo "Video driver root: $VIDEO_DRIVER_ROOT"

# Create output directory
mkdir -p "$BUILD_OUTPUT"

# Execute build in video-driver root directory
cd "$VIDEO_DRIVER_ROOT"

echo "Preparing build environment..."

# Clean up any previous build artifacts first
echo "Cleaning up any previous build artifacts..."
rm -rf debian dkms.conf scripts 2>/dev/null || true

# Check if we need to copy files (avoid copying to same location)
if [ "$PWD" != "$OVERLAY_DIR" ]; then
    # Temporarily copy overlay files to root directory (cleanup after build)
    echo "Copying debian configuration files..."
    cp -r "$OVERLAY_DIR/debian" ./
    cp "$OVERLAY_DIR/dkms.conf" ./

    # Copy scripts directory to root directory
    echo "Copying build scripts..."
    cp -r "$OVERLAY_DIR/scripts" ./
else
    echo "Already in overlay directory, skipping file copy..."
fi

# Set script execution permissions
chmod +x scripts/*.sh
chmod +x debian/rules

echo "Building debian package..."
# Build debian package
dpkg-buildpackage -us -uc -b

echo "Moving build artifacts..."
# Move all generated package files and build artifacts to overlay/build
mv ../video-driver-dkms_* "$BUILD_OUTPUT/" 2>/dev/null || true

echo "Cleaning up temporary files..."
# Clean up temporary files
rm -rf debian dkms.conf scripts
rm -f ../video-driver-dkms_* 2>/dev/null || true

echo "Build completed successfully!"
echo "Package available in: $BUILD_OUTPUT"
ls -la "$BUILD_OUTPUT"
