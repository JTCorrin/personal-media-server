#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

MONGOOSE_VERSION=7.22
UNITY_VERSION=v2.6.1

MONGOOSE_DIR="$ROOT/third_party/mongoose"
UNITY_DIR="$ROOT/third_party/Unity/src"

fetch() {
  local dest="$1"
  local url="$2"
  if [ -f "$dest" ] && [ "${FORCE:-0}" != 1 ]; then
    return 0
  fi
  mkdir -p "$(dirname "$dest")"
  curl -fsSL "$url" -o "$dest"
}

base_mongoose="https://raw.githubusercontent.com/cesanta/mongoose/${MONGOOSE_VERSION}"
base_unity="https://raw.githubusercontent.com/ThrowTheSwitch/Unity/${UNITY_VERSION}/src"

fetch "$MONGOOSE_DIR/mongoose.c" "$base_mongoose/mongoose.c"
fetch "$MONGOOSE_DIR/mongoose.h" "$base_mongoose/mongoose.h"

for file in unity.c unity.h unity_internals.h; do
  fetch "$UNITY_DIR/$file" "$base_unity/$file"
done

echo "third_party ready (mongoose ${MONGOOSE_VERSION}, Unity ${UNITY_VERSION})"
