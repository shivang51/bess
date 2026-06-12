#!/bin/zsh
set -e
./scripts/build_debug.sh $1
./bin/Debug/x64/Bess $@
