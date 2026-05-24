#!/bin/bash
cd build
ctest --output-on-failure --test-dir ../build/tests/
