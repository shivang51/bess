#!/bin/sh
set -e

EXTRA_CMAKE_ARGS=""
if [ "$(uname -s)" = "Darwin" ]; then
    # pybind11-stubgen can't be pip-installed into Homebrew Python (PEP 668).
    EXTRA_CMAKE_ARGS="-G Ninja -DBESS_GENERATE_PY_STUBS=${BSTUBS:-OFF}"
fi

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release $EXTRA_CMAKE_ARGS ..
cmake --build . --parallel $(getconf _NPROCESSORS_ONLN)
cd ..
