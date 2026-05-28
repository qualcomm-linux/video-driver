#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Video Driver DKMS cleanup and uninstall script

set -e

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PACKAGE_NAME="iris-vpu"

# Resolve package version from git tag (single source of truth)
GIT_TAG=$(git -C "$(dirname "$SCRIPT_DIR")" describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
if [ -z "$GIT_TAG" ]; then
    GIT_TAG=$(grep 'PACKAGE_VERSION=' "$SCRIPT_DIR/dkms.conf" 2>/dev/null | cut -d'"' -f2)
fi
PACKAGE_VERSION="${GIT_TAG:-1.0.0}"

show_help() {
    cat << EOF
Video Driver DKMS Cleanup and Uninstall Tool

Usage: $0 [OPTIONS]

OPTIONS:
    --clean-build       Clean build artifacts (pkg-iris-vpu/build directory)
    --uninstall-dkms    Uninstall DKMS package and modules
    --clean-all         Perform complete cleanup (build artifacts + DKMS uninstall)
    --status            Show current DKMS status
    --help              Show this help message

EXAMPLES:
    $0 --clean-build        # Clean build artifacts only
    $0 --uninstall-dkms     # Uninstall DKMS only
    $0 --clean-all          # Complete cleanup
    $0 --status             # Show status

EOF
}

# Clean build artifacts
clean_build() {
    echo "Cleaning build artifacts..."

    # Clean pkg-iris-vpu/build directory
    if [ -d "$SCRIPT_DIR/build" ]; then
        echo "Removing $SCRIPT_DIR/build directory..."
        rm -rf "$SCRIPT_DIR/build"
        echo "Build artifacts cleanup completed"
    else
        echo "No build artifacts directory found"
    fi

    # Clean possible temporary files in root directory
    cd "$(dirname "$SCRIPT_DIR")"
    echo "Checking root directory temporary files..."

    if [ -d "debian" ]; then
        echo "Removing temporary debian/ directory..."
        rm -rf debian
    fi

    if [ -f "dkms.conf" ]; then
        echo "Removing temporary dkms.conf file..."
        rm -f dkms.conf
    fi

    if [ -d "scripts" ]; then
        echo "Removing temporary scripts/ directory..."
        rm -rf scripts
    fi

    # Clean compilation artifacts
    echo "Cleaning compilation artifacts..."
    make clean 2>/dev/null || true
    find . -name "*.ko" -delete 2>/dev/null || true
    find . -name "*.mod" -delete 2>/dev/null || true
    find . -name "*.mod.c" -delete 2>/dev/null || true
    find . -name "*.mod.o" -delete 2>/dev/null || true
    find . -name "*.o" -delete 2>/dev/null || true
    find . -name ".*.cmd" -delete 2>/dev/null || true
    rm -rf .tmp_versions 2>/dev/null || true
    rm -f Module.symvers modules.order 2>/dev/null || true

    echo "Build artifacts cleanup completed"
}

# Uninstall DKMS package
uninstall_dkms() {
    echo "Uninstalling DKMS package and modules..."

    # Check if DKMS package is installed
    if dpkg -l | grep -q "iris-vpu-dkms"; then
        echo "Found installed iris-vpu-dkms package, uninstalling..."

        # Uninstall debian package
        sudo dpkg -r iris-vpu-dkms || true

        echo "Debian package uninstall completed"
    else
        echo "No installed iris-vpu-dkms package found"
    fi

    # Check DKMS status and cleanup
    if command -v dkms >/dev/null 2>&1; then
        echo "Checking DKMS status..."

        # Show current status
        sudo dkms status | grep "$PACKAGE_NAME" || echo "No DKMS modules found"

        # Try to remove all versions of the module
        for version in $(sudo dkms status | grep "$PACKAGE_NAME" | \
                        cut -d',' -f2 | cut -d':' -f1 | sort -u); do
            echo "Removing DKMS module: $PACKAGE_NAME/$version"
            sudo dkms remove "$PACKAGE_NAME/$version" --all || true
        done

        # Clean source directory
        if [ -d "/usr/src/$PACKAGE_NAME-$PACKAGE_VERSION" ]; then
            echo "Removing source directory: /usr/src/$PACKAGE_NAME-$PACKAGE_VERSION"
            sudo rm -rf "/usr/src/$PACKAGE_NAME-$PACKAGE_VERSION"
        fi

        # Unload module (if loaded)
        echo "Checking and unloading loaded modules..."
        if lsmod | grep -q "^video "; then
            echo "Unloading video module..."
            sudo rmmod video || true
        fi

        # Update module dependencies
        echo "Updating module dependencies..."
        sudo depmod -a

        echo "DKMS cleanup completed"
    else
        echo "DKMS not installed, skipping DKMS cleanup"
    fi
}

# Show status
show_status() {
    echo "=== Video Driver DKMS Status ==="
    echo

    # Check debian package status
    echo "Debian package status:"
    if dpkg -l | grep -q "iris-vpu-dkms"; then
        dpkg -l | grep "iris-vpu-dkms"
    else
        echo "  iris-vpu-dkms package not installed"
    fi
    echo

    # Check DKMS status
    if command -v dkms >/dev/null 2>&1; then
        echo "DKMS status:"
        sudo dkms status | grep "$PACKAGE_NAME" || echo "  No DKMS modules found"
    else
        echo "DKMS not installed"
    fi
    echo

    # Check module load status
    echo "Module load status:"
    if lsmod | grep -q "^video "; then
        lsmod | grep "^video "
    else
        echo "  video module not loaded"
    fi
    echo

    # Check source directory
    echo "Source directory:"
    if [ -d "/usr/src/$PACKAGE_NAME-$PACKAGE_VERSION" ]; then
        echo "  /usr/src/$PACKAGE_NAME-$PACKAGE_VERSION exists"
        ls -la "/usr/src/$PACKAGE_NAME-$PACKAGE_VERSION" | head -5
    else
        echo "  /usr/src/$PACKAGE_NAME-$PACKAGE_VERSION does not exist"
    fi
    echo

    # Check build artifacts
    echo "Build artifacts:"
    if [ -d "$SCRIPT_DIR/build" ]; then
        echo "  $SCRIPT_DIR/build exists"
        ls -la "$SCRIPT_DIR/build"
    else
        echo "  $SCRIPT_DIR/build does not exist"
    fi
}

# Complete cleanup
clean_all() {
    echo "Performing complete cleanup..."
    uninstall_dkms
    clean_build
    echo "Complete cleanup finished"
}

# Main function
main() {
    if [ $# -eq 0 ]; then
        show_help
        exit 1
    fi

    case "$1" in
        --clean-build)
            clean_build
            ;;
        --uninstall-dkms)
            uninstall_dkms
            ;;
        --clean-all)
            clean_all
            ;;
        --status)
            show_status
            ;;
        --help)
            show_help
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
