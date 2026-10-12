#!/bin/zsh
set -e

EXTRA_CMAKE_ARGS=()
if [[ "$(uname -s)" == "Darwin" ]]; then
		# pybind11-stubgen can't be pip-installed into Homebrew Python (PEP 668).
		EXTRA_CMAKE_ARGS=(-G Ninja -DBESS_GENERATE_PY_STUBS=${BSTUBS:-OFF})
fi

if [[ "$1" == "--refresh" ]] || [ ! -d "build" ] || [ ! -f "build/CMakeCache.txt" ]; then
    echo "-- Running full CMake configuration..."
		mkdir -p build
		cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DTESTS=${BTESTS:-OFF} -DDOCS=${BDOCS:-OFF} "${EXTRA_CMAKE_ARGS[@]}"
		ln -sf ./build/compile_commands.json .
fi

cmake --build build --parallel $(( $(getconf _NPROCESSORS_ONLN) / 2 ))

# ./scripts/compile_shaders.sh
