#!/usr/bin/env bash
# Build all radapter DEB packages and the ROS plugin .so files.
# Output lands in ${OUT:-out}/.

set -eu

OUT="${OUT:-out}"
mkdir -p "$OUT"

echo "=== radapter native DEBs ==="
docker buildx build -f scripts/Dockerfile.native --target headless-pkg --output="$OUT" .
docker buildx build -f scripts/Dockerfile.native --target gui-pkg      --output="$OUT" .

echo "=== radapter cross DEBs (aarch64) ==="
docker buildx build -f scripts/Dockerfile.cross --target headless-pkg --output="$OUT" .
docker buildx build -f scripts/Dockerfile.cross --target gui-pkg      --output="$OUT" .

echo "=== ROS plugin ==="
docker buildx build -f scripts/Dockerfile.ros --target native-pkg --output="$OUT" .
docker buildx build -f scripts/Dockerfile.ros --target cross-pkg  --output="$OUT" .

echo "=== done ==="
ls -la "$OUT"
