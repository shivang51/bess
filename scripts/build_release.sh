#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

"${SCRIPT_DIR}/bootstrap_deps.sh"

GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
	GENERATOR="Ninja"
fi

if [ -f "${ROOT_DIR}/build/CMakeCache.txt" ]; then
	cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" \
		-DCMAKE_BUILD_TYPE=Release
else
	cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" \
		-G "${GENERATOR}" \
		-DCMAKE_BUILD_TYPE=Release
fi

cmake --build "${ROOT_DIR}/build" --config Release --parallel
