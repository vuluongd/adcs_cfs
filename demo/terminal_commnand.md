```bash
# Light
export LIGHT_APP_BACKEND=i2c
export LIGHT_APP_I2C_DEVICE=/dev/i2c-1

# IMU
export IMU_APP_BACKEND=i2c
export IMU_APP_I2C_DEVICE=/dev/i2c-1

# ACT (bánh đà)
export ACT_APP_BACKEND=gpio
# export ACT_APP_PWM_DIR=/sys/class/pwm/pwmchip0/pwm0
# export ACT_APP_GPIO_ENABLE=23
# export ACT_APP_GPIO_DIR=24
# export ACT_APP_GPIO_BRAKE=25

# Chạy cFS
cd /path/to/cfs/exe
./core-cpu1
```