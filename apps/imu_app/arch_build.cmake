###########################################################
#
# IMU_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the IMU_APP configuration
set(IMU_APP_PLATFORM_CONFIG_FILE_LIST
  imu_app_internal_cfg_values.h
  imu_app_platform_cfg.h
  imu_app_perfids.h
  imu_app_msgids.h
  imu_app_msgid_values.h
)

generate_configfile_set(${IMU_APP_PLATFORM_CONFIG_FILE_LIST})

