#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Cross-compilation script for development and testing
# Uses pkg-video-driver scripts for cross-compilation

set -e

SCRIPT_DIR="$(dirname "$0")"
DKMS_DEBIAN_DIR="$SCRIPT_DIR/pkg-video-driver"

# Default parameters
COMPATIBLE=""
ARCH=""
CROSS_COMPILE=""
KERNEL_SRC=""
OUTPUT_DIR="$SCRIPT_DIR/build-output"
CLEAN_BUILD=false

# Show help information
show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Cross-compile video driver using pkg-video-driver scripts.

OPTIONS:
    --compatible COMPAT     Specify compatible string (e.g., qcom,hamoa-vidc)
    --arch ARCH            Target architecture (default: auto-detect)
    --cross-compile PREFIX  Cross compiler prefix (default: auto-detect)
    --kernel-src PATH      Kernel source path (default: /lib/modules/\$(uname -r)/build)
    --output-dir DIR       Output directory (default: ./build-output)
    --clean                Clean build artifacts before building
    --help                 Show this help message

EXAMPLES:
    # Auto-detect platform and cross-compile
    $0

    # Specify platform
    $0 --compatible qcom,pineapple-vidc

    # Full configuration
    $0 --compatible qcom,sun-vidc \\
       --arch arm64 \\
       --cross-compile aarch64-linux-gnu- \\
       --kernel-src /path/to/kernel \\
       --output-dir /path/to/output

    # Clean build
    $0 --clean

SUPPORTED PLATFORMS:
    hamoa, pineapple, sun, chora, seraph, lemans, nordau, art, canoe,
    ravelin, alor, tuna, kera

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --compatible)
            COMPATIBLE="$2"
            shift 2
            ;;
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --cross-compile)
            CROSS_COMPILE="$2"
            shift 2
            ;;
        --kernel-src)
            KERNEL_SRC="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Set default kernel source path
if [ -z "$KERNEL_SRC" ]; then
    KERNEL_SRC="/lib/modules/$(uname -r)/build"
fi

echo "Video Driver Cross-Compilation Script"
echo "====================================="

# Clean build artifacts
if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build artifacts..."
    make clean 2>/dev/null || true
    rm -rf "$OUTPUT_DIR"
    echo "Clean completed."
    exit 0
fi

# Check if pkg-video-driver scripts exist
if [ ! -f "$DKMS_DEBIAN_DIR/scripts/set-build-env.sh" ]; then
    echo "Error: pkg-video-driver scripts not found. Please ensure pkg-video-driver directory exists."
    exit 1
fi

# Setup build environment
echo "Setting up build environment..."
if [ -n "$COMPATIBLE" ]; then
    echo "Using specified compatible: $COMPATIBLE"
    export VIDEO_COMPATIBLE="$COMPATIBLE"
fi

# Manually set cross-compilation parameters (if specified)
if [ -n "$ARCH" ]; then
    export ARCH="$ARCH"
fi
if [ -n "$CROSS_COMPILE" ]; then
    export CROSS_COMPILE="$CROSS_COMPILE"
fi

# Call pkg-video-driver script to setup environment
source "$DKMS_DEBIAN_DIR/scripts/set-build-env.sh" "$COMPATIBLE"

# Set kernel source path
export KERNEL_SRC="$KERNEL_SRC"

echo ""
echo "Build Configuration:"
echo "  Compatible: ${COMPATIBLE:-auto-detect}"
echo "  Architecture: ${ARCH:-native}"
echo "  Cross Compiler: ${CROSS_COMPILE:-none}"
echo "  Kernel Source: $KERNEL_SRC"
echo "  Output Directory: $OUTPUT_DIR"
echo ""

# Check kernel source path
if [ ! -d "$KERNEL_SRC" ]; then
    echo "Error: Kernel source directory not found: $KERNEL_SRC"
    echo "Please install kernel headers or specify correct path with --kernel-src"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Execute build
echo "Starting cross-compilation..."
make -C "$KERNEL_SRC" M="$(pwd)" modules

# Copy build artifacts to output directory
echo "Copying build artifacts to output directory..."
find . -name "*.ko" -exec cp {} "$OUTPUT_DIR/" \;
find . -name "*.mod" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null || true

echo ""
echo "Cross-compilation completed successfully!"
echo "Build artifacts available in: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR"
