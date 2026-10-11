#!/bin/zsh
set -e
./scripts/build_debug.sh $1
./bin/Debug/*/Bess([1]) $@
