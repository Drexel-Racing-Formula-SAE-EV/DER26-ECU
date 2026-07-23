#!/usr/bin/env bash
set -euo pipefail

echo "Checking for forbidden generated artifacts..."

forbidden_dirs=(
  "Debug"
  "Release"
  "build"
  "host_tests/build"
  "ci_artifacts"
)

for path in "${forbidden_dirs[@]}"; do
  if [[ -e "$path" ]]; then
    echo "Forbidden generated directory committed or present: $path"
    exit 1
  fi
done

bad_files=$(find . -path "./.git" -prune -o \( \
  -name "*.elf" -o -name "*.bin" -o -name "*.hex" -o -name "*.map" -o -name "*.list" -o \
  -name "*.lst" -o -name "*.o" -o -name "*.d" -o -name "*.su" -o -name "*.log" -o \
  -name "*.tmp" -o -name "*.plist" -o -name "*.bak" -o -name "*.orig" -o -name "*.rej" \) -print)

if [[ -n "$bad_files" ]]; then
  echo "Forbidden generated files found:"
  echo "$bad_files"
  exit 1
fi

echo "Repo hygiene check passed."
