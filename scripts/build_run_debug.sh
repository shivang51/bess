#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

"${SCRIPT_DIR}/build_debug.sh"

if [ -x "${ROOT_DIR}/bin/Debug/x64/Bess" ]; then
	"${ROOT_DIR}/bin/Debug/x64/Bess" "$@"
elif [ -x "${ROOT_DIR}/bin/Debug/x64/Bess.exe" ]; then
	"${ROOT_DIR}/bin/Debug/x64/Bess.exe" "$@"
else
	echo "Unable to find debug executable in bin/Debug/x64" >&2
	exit 1
fi
