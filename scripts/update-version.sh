#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ] && [ "$#" -ne 3 ]; then
  echo "Usage: $0 version.h [year month]" >&2
  exit 1
fi

file=$1
if [ "$#" -eq 3 ]; then
  year=$2
  month=$3
else
  IFS=- read -r year month <<< "$(date -u +%Y-%m)"
fi
if [[ ! $year =~ ^[0-9]{4}$ || ! $month =~ ^[0-9]{1,2}$ ]]; then
  echo "Invalid year or month: $year $month" >&2
  exit 1
fi
year=$((10#$year))
month=$((10#$month))
if (( year < 2000 || year > 2099 || month < 1 || month > 12 )); then
  echo "Year or month is outside the supported range: $year $month" >&2
  exit 1
fi
year=$((year - 2000))

values=$(awk '
  $1 == "#define" && ($2 == "MAJOR_VERSION" || $2 == "MINOR_VERSION" || $2 == "RELEASE_VERSION") {
    if (NF != 3 || $3 !~ /^[0-9]+$/ || seen[$2]++) exit 1
    value[$2] = $3
  }
  END {
    if (!seen["MAJOR_VERSION"] || !seen["MINOR_VERSION"] || !seen["RELEASE_VERSION"]) exit 1
    print value["MAJOR_VERSION"], value["MINOR_VERSION"], value["RELEASE_VERSION"]
  }
' "$file") || { echo "Invalid version definitions in $file" >&2; exit 1; }
read -r old_year old_month patch <<< "$values"
old_year=$((10#$old_year))
old_month=$((10#$old_month))
patch=$((10#$patch))

if (( old_year > 99 || old_month < 1 || old_month > 12 || patch > 255 )); then
  echo "Invalid existing calendar version in $file" >&2
  exit 1
fi
if (( old_year > year || (old_year == year && old_month > month) )); then
  echo "Refusing to move the version backwards" >&2
  exit 1
elif (( old_year == year && old_month == month )); then
  patch=$((patch + 1))
else
  patch=0
fi
if (( patch > 255 )); then
  echo "Patch version exceeds the VSCP API's 8-bit limit" >&2
  exit 1
fi

tmp=$(mktemp "${file}.XXXXXX")
trap 'rm -f "$tmp"' EXIT
awk -v year="$year" -v month="$month" -v patch="$patch" '
  $1 == "#define" && $2 == "MAJOR_VERSION" { $3 = year }
  $1 == "#define" && $2 == "MINOR_VERSION" { $3 = month }
  $1 == "#define" && $2 == "RELEASE_VERSION" { $3 = patch }
  { print }
' "$file" > "$tmp"
chmod --reference="$file" "$tmp"
mv "$tmp" "$file"
printf '%02d.%d.%d\n' "$year" "$month" "$patch"
