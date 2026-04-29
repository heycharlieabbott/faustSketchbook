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

dest_dir="${HOME}/Library/Application Support/REAPER/Effects"
dest_file="${dest_dir}/$(basename "${output}")"

mkdir -p "${dest_dir}"
cp "${output}" "${dest_file}"
echo "Compiled ${output} and copied to ${dest_file}."
