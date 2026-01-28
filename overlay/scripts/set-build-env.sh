#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Setup platform configuration based on compatible string
setup_platform_config() {
    local compatible="$1"

    if [ -z "$compatible" ]; then
        echo "Error: No compatible string provided" >&2
        exit 1
    fi

    echo "Setting up build environment for: $compatible"

    # Set macros based on compatible string (iris format only)
    case "$compatible" in
        "qcom,x1e80100-iris")
            #export CONFIG_ARCH_X1E80100=y
            #export CONFIG_MSM_VIDC_HAMOA=y
            #export CONFIG_MSM_VIDC_QLI=y
            echo "Platform: HAMOA (X1E80100)"
            ;;
        "qcom,sa8775p-iris")
            #export CONFIG_ARCH_LEMANS=y
            #export CONFIG_MSM_VIDC_LEMANS=y
            #export CONFIG_QLI_VIDC_SA8775P=y
            echo "Platform: LEMANS"
            ;;
        *)
            echo "Error: Unsupported compatible '$compatible'" >&2
            echo "Supported platforms (iris format only):" >&2
            echo "  - qcom,x1e80100-iris (HAMOA)" >&2
            echo "  - qcom,sa8775p-iris (LEMANS)" >&2
            ;;
    esac

    # Setup cross-compilation (unified for all platforms)
    setup_cross_compile
}

# Setup cross-compilation environment
setup_cross_compile() {
    local host_arch=$(uname -m)

    echo "Host architecture: $host_arch"

    # Setup cross-compilation if not aarch64 host
    if [ "$host_arch" != "aarch64" ]; then
        export ARCH=arm64

        # Find available cross compiler
        for cc in aarch64-linux-gnu- aarch64-none-linux-gnu- arm64-linux-gnu-; do
            if command -v "${cc}gcc" >/dev/null 2>&1; then
                export CROSS_COMPILE="$cc"
                echo "Using cross compiler: $cc"
                return 0
            fi
        done

        echo "Error: No suitable cross compiler found" >&2
        echo "Please install: sudo apt install gcc-aarch64-linux-gnu" >&2
        exit 1
    else
        echo "Native aarch64 build"
    fi
}

# Main function
main() {
    local compatible="$1"

    # Auto-detect if no compatible provided
    if [ -z "$compatible" ]; then
        echo "Auto-detecting platform..."
        compatible=$($(dirname "$0")/detect-platform.sh)
    fi

    setup_platform_config "$compatible"

    echo "Build environment configured successfully"
    echo "Compatible: $compatible"
    echo "ARCH: ${ARCH:-native}"
    echo "CROSS_COMPILE: ${CROSS_COMPILE:-none}"

    # Export environment variables for make
    export VIDEO_ROOT=$(pwd)
}

main "$@"
