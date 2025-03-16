#!/bin/zsh
mkdir build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -s ./compile_commands.json ..
make -j8
cd ..
