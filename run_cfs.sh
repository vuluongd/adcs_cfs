#!/bin/bash
export ACT_APP_BACKEND=gpio
export ACT_APP_PWM_DIR=/sys/class/pwm/pwmchip0/pwm0
export ACT_APP_GPIO_ENABLE=none
export ACT_APP_GPIO_DIR=none
export ACT_APP_GPIO_BRAKE=none
export ACT_APP_DUTY_MAX_PCT=30
export IMU_APP_BACKEND=i2c
export IMU_APP_I2C_DEVICE=/dev/i2c-1

cd ~/adcs_cfs/build-native_std/exe/cpu1 || exit 1
sudo -E ./core-cpu1
