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
		-Wno-dev \
		-Wno-deprecated \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
else
	cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" \
		-G "${GENERATOR}" \
		-Wno-dev \
		-Wno-deprecated \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

cmake --build "${ROOT_DIR}/build" --config Debug --parallel -j4

if [ -f "${ROOT_DIR}/build/compile_commands.json" ]; then
	if [ ! "${ROOT_DIR}/build/compile_commands.json" -ef "${ROOT_DIR}/compile_commands.json" ]; then
		cp "${ROOT_DIR}/build/compile_commands.json" "${ROOT_DIR}/compile_commands.json"
	fi
fi
