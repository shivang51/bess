#!/bin/bash
cd build
mkdir -p tests_output
ctest --output-on-failure -O tests_output/tests.log
