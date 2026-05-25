#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

set -e

echo "Starting DKMS build for iris-vpu..."

# Get kernel information
KERNEL_VERSION=$(uname -r)
KERNEL_ARCH=$(uname -m)
echo "Target kernel: $KERNEL_VERSION ($KERNEL_ARCH)"

# Minimal custom kernel detection for production
if [[ "$KERNEL_VERSION" == *"-dirty" ]] || [[ "$KERNEL_VERSION" == *"rc"* ]]; then
    echo "Custom/development kernel detected, enabling basic compatibility..."
    export DKMS_DISABLE_APPORT=1
    export IGNORE_CC_MISMATCH=1
fi

# Detect platform from device tree
echo "Detecting platform from device tree..."
COMPATIBLE=$($(dirname "$0")/detect-platform.sh)

if [ -z "$COMPATIBLE" ]; then
    echo "Error: Failed to detect platform" >&2
    exit 1
fi

echo "Detected compatible: $COMPATIBLE"

# Setup build environment
echo "Setting up build environment..."
source $(dirname "$0")/set-build-env.sh "$COMPATIBLE"

# Set build arguments to handle compiler version mismatch
MAKE_ARGS="M=$(pwd) VIDEO_ROOT=$(pwd) modules"
if [[ "$KERNEL_VERSION" == *"-dirty" ]]; then
    MAKE_ARGS="$MAKE_ARGS CONFIG_CC_VERSION_TEXT=\"\""
fi

# Build the module
echo "Building kernel module..."
make -C /lib/modules/$(uname -r)/build $MAKE_ARGS

echo "Build completed successfully!"
