#!/bin/bash
cmake -S tools/act_demo -B build-act-unit
cmake --build build-act-unit -j4
ctest --test-dir build-act-unit --output-on-failure   # phải "100% tests passed"
./build-act-unit/act_demo          # +150, đảo -100, dừng
./build-act-unit/act_demo 80       # giữ 80 rad/s
