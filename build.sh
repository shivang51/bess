#!/bin/zsh
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ ..
make -j8
cd ..
