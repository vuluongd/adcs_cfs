#!/bin/bash
cmake -S tools/act_demo -B build-act-unit
cmake --build build-act-unit -j4


sudo ACT_APP_BACKEND=gpio \
     ACT_APP_GPIO_ENABLE=none \
     ACT_APP_GPIO_DIR=none \
     ACT_APP_GPIO_BRAKE=none \
     ACT_APP_PWM_FREQ_HZ=1000
     ACT_APP_DUTY_MAX_PCT=30
     ./build-act-unit/act_demo 90
