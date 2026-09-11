#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

set -euo pipefail

# BASE_SHA = commit to compare against (PR base)
# HEAD_SHA = commit being tested (PR head)
BASE_SHA="${BASE_SHA:-}"
HEAD_SHA="${HEAD_SHA:-HEAD}"

if [[ -z "$BASE_SHA" ]]; then
    echo "ERROR: BASE_SHA is not set. Set BASE_SHA to the base commit to diff against."
    exit 1
fi

echo "Running UAPI + driver checks between:"
echo "  base = $BASE_SHA"
echo "  head = $HEAD_SHA"
echo

changed_files=$(git diff --name-only --diff-filter=AM "$BASE_SHA" "$HEAD_SHA")
if [[ -z "$changed_files" ]]; then
    echo "No changed files; skipping checks."
    exit 0
fi

exit_status=0

###############################################################################
# 1. UAPI header checks (v4l2_vidc_extensions.h)
###############################################################################

uapi_header="include/uapi/vidc/media/v4l2_vidc_extensions.h"

if echo "$changed_files" | grep -q "^$uapi_header$"; then
    echo "UAPI header changed: $uapi_header"
    echo "Checking for removed struct/enums/defines that may break ABI ..."

    pre_uapi=$(mktemp)
    post_uapi=$(mktemp)

    git show "$BASE_SHA:$uapi_header" > "$pre_uapi" 2>/dev/null || true
    git show "$HEAD_SHA:$uapi_header" > "$post_uapi" 2>/dev/null || true

    if [[ ! -s "$pre_uapi" ]]; then
        echo "Note: UAPI header appears to be newly added; skipping ABI removal check."
    else
        removed_lines=$(diff -u "$pre_uapi" "$post_uapi" | grep '^-' | grep -v '^---' || true)

        if echo "$removed_lines" \
            | grep -E '^\-.*(struct|enum|#define|V4L2_.*|VIDC_.*)' >/dev/null; then
            echo "ERROR: Potential ABI break in $uapi_header – definitions removed:"
            echo "$removed_lines"
            exit_status=1
        fi
    fi

    rm -f "$pre_uapi" "$post_uapi"
    echo
fi

###############################################################################
# 2. Sysfs usage checks in modified C files
###############################################################################

echo "Checking for sysfs usage in modified C files ..."

while read -r f; do
    [[ -z "$f" ]] && continue
    [[ "$f" != *.c ]] && continue

    if grep -qE 'sysfs_create_file|device_create_file|sysfs_remove_file' "$f"; then
        echo "ERROR: sysfs interface usage detected in modified file: $f"
        echo "       Policy: avoid adding/modifying sysfs in this driver."
        exit_status=1
    fi
done <<< "$changed_files"

echo

###############################################################################
# 3. Optional: module_param checks
###############################################################################

echo "Checking module_param definitions ..."

if echo "$changed_files" | grep -q '\.c$'; then
    tmp_diff=$(mktemp)

    git diff "$BASE_SHA" "$HEAD_SHA" -- '*.c' \
        | grep -E '^[\+\-].*module_param' > "$tmp_diff" 2>/dev/null || true

    if grep -E '^\-.*module_param' "$tmp_diff" >/dev/null; then
        echo "ERROR: module_param removed/modified in diff:"
        grep -E '^\-.*module_param' "$tmp_diff"
        exit_status=1
    fi

    rm -f "$tmp_diff"
fi

echo
if [[ "$exit_status" -ne 0 ]]; then
    echo "UAPI/driver checks FAILED."
else
    echo "UAPI/driver checks PASSED."
fi

exit "$exit_status"
