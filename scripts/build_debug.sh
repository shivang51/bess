#!/bin/zsh
set -e
if [[ "$1" == "--refresh" ]] || [ ! -d "build" ]; || [! -f "build/CMakeCache.txt"] then
    echo "-- Running full CMake configuration..."
		mkdir -p build
		cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DTESTS=${BTESTS:-OFF} -DDOCS=${BDOCS:-OFF}
		ln -sf ./build/compile_commands.json .
fi

cmake --build build --parallel $(( $(nproc) / 2 ))

# ./scripts/compile_shaders.sh
