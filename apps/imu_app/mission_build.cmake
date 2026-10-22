###########################################################
#
# IMU_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the IMU_APP configuration
set(IMU_APP_MISSION_CONFIG_FILE_LIST
  imu_app_fcncode_values.h
  imu_app_interface_cfg_values.h
  imu_app_mission_cfg.h
  imu_app_perfids.h
  imu_app_msg.h
  imu_app_msgdefs.h
  imu_app_msgstruct.h
  imu_app_tbl.h
  imu_app_tbldefs.h
  imu_app_tblstruct.h
  imu_app_topicid_values.h
)

generate_configfile_set(${IMU_APP_MISSION_CONFIG_FILE_LIST})

