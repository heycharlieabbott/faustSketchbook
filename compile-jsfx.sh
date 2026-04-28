#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <file.dsp|basename>" >&2
  exit 1
fi

input=$1
if [[ "${input}" != *.dsp ]]; then
  input="${input}.dsp"
fi

base=${input%.dsp}
output="${base}.jsfx"

faust -lang jsfx "${input}" -o "${output}"

if ! command -v pbcopy >/dev/null 2>&1; then
  echo "Error: pbcopy is required to copy output to clipboard." >&2
  exit 1
fi

pbcopy < "${output}"
echo "Compiled ${output} and copied contents to clipboard."
