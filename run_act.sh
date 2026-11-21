#!/bin/bash
cmake -S tools/act_demo -B build-act-unit
cmake --build build-act-unit -j4


sudo ACT_APP_BACKEND=gpio \
     ACT_APP_GPIO_ENABLE=none \
     ACT_APP_GPIO_DIR=536 \
     ACT_APP_GPIO_BRAKE=none \
     ACT_APP_PWM_FREQ_HZ=20000\
     ACT_APP_DUTY_MAX_PCT=90\
     ACT_APP_RAD_S_PER_DUTY=0.5\
     ./build-act-unit/act_demo 90
