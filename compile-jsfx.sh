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

exec faust -lang jsfx "${input}" -o "${output}"
