#!/bin/zsh
set -e
mkdir -p build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DTESTS=${BTESTS:-OFF} -DDOCS=${BDOCS:-OFF}
ln -sf ./build/compile_commands.json ..
make -j4
cd ..

# ./scripts/compile_shaders.sh
