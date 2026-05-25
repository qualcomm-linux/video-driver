#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Universal build wrapper for video driver
# Works in both Ubuntu DKMS and Yocto kernel build environments

set -e

echo "Starting video driver build (Universal Mode)..."

# Get kernel information
KERNEL_VERSION=$(uname -r)
KERNEL_ARCH=$(uname -m)
echo "Target kernel: $KERNEL_VERSION ($KERNEL_ARCH)"

# Detect build environment
BUILD_ENV="generic"
if [ -n "$YOCTO_VERSION" ] || [ -n "$BB_ENV_EXTRAWHITE" ]; then
    BUILD_ENV="yocto"
    echo "Build environment: Yocto"
elif [ -d "/var/lib/dkms" ]; then
    BUILD_ENV="dkms"
    echo "Build environment: DKMS (Ubuntu)"
else
    echo "Build environment: Generic"
fi

# Handle custom/development kernels
if [[ "$KERNEL_VERSION" == *"-dirty" ]] || [[ "$KERNEL_VERSION" == *"rc"* ]]; then
    echo "Custom/development kernel detected, enabling compatibility mode..."
    export DKMS_DISABLE_APPORT=1
    export IGNORE_CC_MISMATCH=1
fi

# Detect platform using unified detection
echo "Detecting platform from device tree..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPATIBLE=$("$SCRIPT_DIR/detect-platform.sh")

if [ -z "$COMPATIBLE" ]; then
    echo "Error: Failed to detect platform" >&2
    exit 1
fi

echo "Detected compatible: $COMPATIBLE"

# Setup build environment
echo "Setting up build environment..."
source "$SCRIPT_DIR/set-build-env.sh" "$COMPATIBLE"

# Set build arguments
MAKE_ARGS="M=$(pwd) VIDEO_ROOT=$(pwd) modules"

# Handle compiler version mismatch for custom kernels
if [[ "$KERNEL_VERSION" == *"-dirty" ]]; then
    MAKE_ARGS="$MAKE_ARGS CONFIG_CC_VERSION_TEXT=\"\""
fi

# Determine kernel build directory based on environment
if [ "$BUILD_ENV" = "yocto" ]; then
    # In Yocto, use KERNEL_SRC if available, otherwise standard path
    KERNEL_BUILD_DIR="${KERNEL_SRC:-/lib/modules/$KERNEL_VERSION/build}"
else
    # In Ubuntu/DKMS, use standard path
    KERNEL_BUILD_DIR="/lib/modules/$KERNEL_VERSION/build"
fi

echo "Using kernel build directory: $KERNEL_BUILD_DIR"

# Build the module
echo "Building kernel module..."
make -C "$KERNEL_BUILD_DIR" $MAKE_ARGS

echo "Build completed successfully!"

# Verify module was built
MODULE_FILE="video/iris_vpu.ko"
if [ -f "$MODULE_FILE" ]; then
    MODULE_SIZE=$(stat -c%s "$MODULE_FILE")
    echo "Module built: $MODULE_FILE ($MODULE_SIZE bytes)"
else
    echo "Error: Module file not found: $MODULE_FILE" >&2
    exit 1
fi
