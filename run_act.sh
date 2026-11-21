#!/bin/bash
cmake -S tools/act_demo -B build-act-unit
cmake --build build-act-unit -j4


sudo ACT_APP_BACKEND=gpio \
     ACT_APP_GPIO_ENABLE=none \
     ACT_APP_GPIO_DIR=none \
     ACT_APP_GPIO_BRAKE=none \
     ./build-act-unit/act_demo 30
