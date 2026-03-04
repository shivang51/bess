#!/bin/zsh
set -e
./scripts/build_debug.sh
./bin/Debug/x64/Bess $@
