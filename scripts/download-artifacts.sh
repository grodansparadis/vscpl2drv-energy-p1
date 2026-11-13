#!/usr/bin/env bash
#
# download-artifacts.sh
#
# Download the newest successful CI build packages of vscpl2drv-energy-p1 and
# check them against the SHA256SUMS file in each artifact.
#
# Requires the GitHub CLI (https://cli.github.com), signed in with
# "gh auth login". GitHub only serves workflow artifacts to signed-in users,
# also for public repositories. Artifacts are kept for 90 days.
#
# Usage:
#   scripts/download-artifacts.sh [-b branch] [-o dir] [-R owner/repo] [platform...]
#
# Platforms: linux macos windows raspberry-pi-arm64 raspberry-pi-armhf
# Without platforms, all are downloaded.
#
# Examples:
#   scripts/download-artifacts.sh
#   scripts/download-artifacts.sh -o /tmp/p1 linux raspberry-pi-arm64
#

set -euo pipefail

REPO="grodansparadis/vscpl2drv-energy-p1"
BRANCH="main"
OUTDIR="artifacts"
ALL_PLATFORMS=(linux macos windows raspberry-pi-arm64 raspberry-pi-armhf)

usage() {
  sed -n '3,21p' "$0" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while getopts "b:o:R:h" opt; do
  case "$opt" in
    b) BRANCH="$OPTARG" ;;
    o) OUTDIR="$OPTARG" ;;
    R) REPO="$OPTARG" ;;
    h) usage 0 ;;
    *) usage 1 ;;
  esac
done
shift $((OPTIND - 1))

platforms=("$@")
if [ ${#platforms[@]} -eq 0 ]; then
  platforms=("${ALL_PLATFORMS[@]}")
fi

# Name of the artifact each platform's workflow uploads
artifact_name() {
  case "$1" in
    linux) echo "vscpl2drv-energy-p1-Linux" ;;
    macos) echo "vscpl2drv-energy-p1-macOS" ;;
    windows) echo "vscpl2drv-energy-p1-Windows" ;;
    raspberry-pi-arm64) echo "vscpl2drv-energy-p1-Raspberry-Pi-arm64" ;;
    raspberry-pi-armhf) echo "vscpl2drv-energy-p1-Raspberry-Pi-armhf" ;;
    *) return 1 ;;
  esac
}

verify() {
  if command -v sha256sum >/dev/null; then
    sha256sum -c SHA256SUMS
  else
    shasum -a 256 -c SHA256SUMS
  fi
}

if ! command -v gh >/dev/null; then
  echo "The GitHub CLI (gh) is required: https://cli.github.com" >&2
  exit 1
fi
if ! gh auth status >/dev/null 2>&1; then
  echo "Sign in to GitHub first with: gh auth login" >&2
  exit 1
fi

failed=0
for platform in "${platforms[@]}"; do
  if ! name=$(artifact_name "$platform"); then
    echo "Unknown platform '$platform'. Use one of: ${ALL_PLATFORMS[*]}" >&2
    failed=1
    continue
  fi

  run=$(gh run list -R "$REPO" --workflow "build-$platform.yml" --branch "$BRANCH" \
          --status success --limit 1 --json databaseId,headSha,createdAt \
          --jq '.[0] | select(. != null) | "\(.databaseId) \(.headSha[0:7]) \(.createdAt)"')
  if [ -z "$run" ]; then
    echo "$platform: no successful build on $BRANCH" >&2
    failed=1
    continue
  fi
  read -r run_id sha created <<<"$run"

  dir="$OUTDIR/$platform"
  rm -rf "$dir"
  mkdir -p "$dir"
  echo "$platform: run $run_id, commit $sha, built $created"
  if ! gh run download "$run_id" -R "$REPO" -n "$name" -D "$dir"; then
    echo "$platform: download failed (artifacts expire after 90 days)" >&2
    failed=1
    continue
  fi

  if [ -f "$dir/SHA256SUMS" ]; then
    if ! (cd "$dir" && verify); then
      echo "$platform: CHECKSUM MISMATCH" >&2
      failed=1
    fi
  else
    echo "$platform: no SHA256SUMS in this artifact, not verified" >&2
  fi
done

exit "$failed"
