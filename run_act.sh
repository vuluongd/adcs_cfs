#!/bin/bash
cmake -S tools/act_demo -B build-act-unit
cmake --build build-act-unit -j4
ctest --test-dir build-act-unit --output-on-failure   # phải "100% tests passed"

ACT_APP_BACKEND=gpio ./build-act-unit/act_demo 50
