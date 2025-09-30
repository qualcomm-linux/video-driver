#!/bin/bash
set -e

echo "🩹 Patching iris-video-dlkm_git.bb to fix SRCREV..."

RECIPE_PATH="meta-qcom/recipes-kernel/iris-video-module/iris-video-dlkm_git.bb"

if [ -f "$RECIPE_PATH" ]; then
  # Replace existing SRCREV line
  sed -i -E 's/^[[:space:]]*SRCREV[[:space:]]*=.*$/SRCREV = "${AUTOREV}"/' "$RECIPE_PATH"

  # If no SRCREV line exists, append it
  if ! grep -q '^[[:space:]]*SRCREV[[:space:]]*=' "$RECIPE_PATH"; then
    echo 'SRCREV = "${AUTOREV}"' >> "$RECIPE_PATH"
    cat "$RECIPE_PATH"
  fi

  echo "✅ SRCREV patch applied successfully."
else
  echo "❌ Recipe file not found: $RECIPE_PATH"
  exit 1
fi

