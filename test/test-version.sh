#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
file="$tmp/version.h"
trap 'rm -f "$file" "$tmp/before"; rmdir "$tmp"' EXIT

make_version() {
  printf '#define MAJOR_VERSION %s\n#define MINOR_VERSION %s\n#define RELEASE_VERSION %s\n#define BUILD_VERSION 0\n' "$1" "$2" "$3" > "$file"
}

check_version() {
  actual=$(bash "$root/scripts/update-version.sh" "$file" "$1" "$2")
  if [ "$actual" != "$3" ]; then
    echo "Expected $3, got $actual" >&2
    exit 1
  fi
}

make_version 26 11 0
check_version 2026 11 26.11.1
check_version 2026 12 26.12.0
check_version 2027 01 27.1.0

make_version 26 11 254
check_version 2026 11 26.11.255
cp "$file" "$tmp/before"
if bash "$root/scripts/update-version.sh" "$file" 2026 11; then
  echo "Expected patch overflow to fail" >&2
  exit 1
fi
cmp "$file" "$tmp/before"
if bash "$root/scripts/update-version.sh" "$file" 2026 10; then
  echo "Expected a backwards version to fail" >&2
  exit 1
fi
cmp "$file" "$tmp/before"
make_version 8 12 3
check_version 2009 01 09.1.0
if bash "$root/scripts/update-version.sh" "$file" 2100 01; then
  echo "Expected unsupported century to fail" >&2
  exit 1
fi
echo '#define RELEASE_VERSION 2' >> "$file"
if bash "$root/scripts/update-version.sh" "$file" 2026 11; then
  echo "Expected duplicate definitions to fail" >&2
  exit 1
fi

echo "Version tests passed"
