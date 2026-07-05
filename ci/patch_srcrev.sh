#!/usr/bin/env bash
set -euo pipefail

VIDEO_DRIVER_DIR="${1:-$(pwd)}"
META_QCOM_DIR="${2:-$(dirname "$VIDEO_DRIVER_DIR")/meta-qcom}"
PR_REPO="${3:-}"

DEFAULT_REPO="qualcomm-linux/video-driver"

echo "VIDEO_DRIVER_DIR=${VIDEO_DRIVER_DIR}"
echo "META_QCOM_DIR=${META_QCOM_DIR}"
echo "PR_REPO=${PR_REPO:-<empty>}"

if [ ! -d "${VIDEO_DRIVER_DIR}/.git" ]; then
  echo "❌ video-driver repo not found: ${VIDEO_DRIVER_DIR}"
  exit 1
fi

if [ ! -d "${META_QCOM_DIR}/.git" ]; then
  echo "❌ meta-qcom repo not found: ${META_QCOM_DIR}"
  exit 1
fi

mapfile -t RECIPE_CANDIDATES < <(
  find "${META_QCOM_DIR}" -type f \
    \( -name 'iris-video-dlkm_*.bb' -o -name 'iris-video-dlkm_git.bb' \) | sort
)

if [ "${#RECIPE_CANDIDATES[@]}" -eq 0 ]; then
  echo "❌ No iris-video-dlkm recipe found under ${META_QCOM_DIR}"
  exit 1
fi

if [ "${#RECIPE_CANDIDATES[@]}" -gt 1 ]; then
  echo "⚠️ Multiple recipe candidates found:"
  printf ' - %s\n' "${RECIPE_CANDIDATES[@]}"
  echo "Using the first match."
fi

RECIPE_PATH="${RECIPE_CANDIDATES[0]}"
VIDEO_DRIVER_SHA=$(git -C "${VIDEO_DRIVER_DIR}" rev-parse HEAD)

if [ -n "${PR_REPO}" ] && [ "${PR_REPO}" != "${DEFAULT_REPO}" ]; then
  TARGET_REPO="${PR_REPO}"
else
  TARGET_REPO="${DEFAULT_REPO}"
fi

TARGET_GIT_URI="git://github.com/${TARGET_REPO}.git"

echo "RECIPE_PATH=${RECIPE_PATH}"
echo "VIDEO_DRIVER_SHA=${VIDEO_DRIVER_SHA}"
echo "TARGET_GIT_URI=${TARGET_GIT_URI}"

echo "🩹 Patching SRC_URI to use exact repo + nobranch=1"
perl -0pi -e '
  s#git://github\.com/[^;"\s]+\.git;protocol=https(?:;branch=[^;\s"]+)?(?:;tag=[^;\s"]+)?(?:;nobranch=1)?#'"${TARGET_GIT_URI}"';protocol=https;nobranch=1#g
' "${RECIPE_PATH}"

echo "🩹 Patching SRCREV to checked-out video-driver SHA"
if grep -q '^[[:space:]]*SRCREV[[:space:]]*=' "${RECIPE_PATH}"; then
  sed -i -E "s|^[[:space:]]*SRCREV[[:space:]]*=.*$|SRCREV = \"${VIDEO_DRIVER_SHA}\"|" "${RECIPE_PATH}"
else
  echo "SRCREV = \"${VIDEO_DRIVER_SHA}\"" >> "${RECIPE_PATH}"
fi

# ── NEW: remove patches already present in the checked-out source ──────────
echo "🩹 Removing patches already merged into source tree..."
sed -i '/0001-video-driver-copy-struct-v4l2_format-by-assignment\.patch/d' "${RECIPE_PATH}"
echo "✅ Stale patch entries removed"
# ───────────────────────────────────────────────────────────────────────────

echo "✅ Recipe patched successfully"
echo "=== Patched recipe preview ==="
grep -nE 'SRC_URI|SRCREV|\.patch' "${RECIPE_PATH}" || true
