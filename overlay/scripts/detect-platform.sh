#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Unified platform detection for all build environments
# Uses device tree compatible string detection for both Ubuntu and Yocto

# Function to detect platform from device tree
detect_platform_devicetree() {
    local compatible=""

    # Try different possible paths for video codec device tree node
    local dt_paths=(
        "/proc/device-tree/soc@0/video-codec@aa00000/compatible"
        "/proc/device-tree/soc/video-codec@aa00000/compatible"
        "/proc/device-tree/video-codec@aa00000/compatible"
        "/proc/device-tree/soc@0/qcom,vidc@aa00000/compatible"
        "/proc/device-tree/soc/qcom,vidc@aa00000/compatible"
        "/proc/device-tree/qcom,vidc@aa00000/compatible"
    )

    for path in "${dt_paths[@]}"; do
        if [ -f "$path" ]; then
            # Use strings to extract null-terminated strings and find first iris compatible
            compatible=$(strings "$path" | grep -m1 '\-iris$')
            if [ -n "$compatible" ]; then
                echo "$compatible"
                return 0
            fi
        fi
    done

    # If no compatible found, return empty
    echo ""
    return 1
}

# Main platform detection function
detect_platform() {
    local manual_platform="${PLATFORM_OVERRIDE:-}"

    # Check for manual override first
    if [ -n "$manual_platform" ]; then
        echo "$manual_platform"
        return 0
    fi

    # Always use device tree detection for all environments
    detect_platform_devicetree
}

# Main execution
compatible=$(detect_platform)
if [ -n "$compatible" ]; then
    echo "$compatible"
    exit 0
else
    echo "Error: Could not detect iris-format compatible from device tree" >&2
    echo "Tried paths:" >&2
    echo "  /proc/device-tree/soc@0/video-codec@aa00000/compatible" >&2
    echo "  /proc/device-tree/soc/video-codec@aa00000/compatible" >&2
    echo "  /proc/device-tree/video-codec@aa00000/compatible" >&2
    echo "  /proc/device-tree/soc@0/qcom,vidc@aa00000/compatible" >&2
    echo "  /proc/device-tree/soc/qcom,vidc@aa00000/compatible" >&2
    echo "  /proc/device-tree/qcom,vidc@aa00000/compatible" >&2
    echo "Note: Only iris-format compatible strings are supported" >&2
    echo "You can override with: PLATFORM_OVERRIDE=qcom,platform-iris $0" >&2
    exit 1
fi
