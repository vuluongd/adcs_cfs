###########################################################
#
# CAMERA_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the CAMERA_APP configuration
set(CAMERA_APP_MISSION_CONFIG_FILE_LIST
  camera_app_fcncode_values.h
  camera_app_interface_cfg_values.h
  camera_app_mission_cfg.h
  camera_app_perfids.h
  camera_app_msg.h
  camera_app_msgdefs.h
  camera_app_msgstruct.h
  camera_app_tbl.h
  camera_app_tbldefs.h
  camera_app_tblstruct.h
  camera_app_topicid_values.h
)

generate_configfile_set(${CAMERA_APP_MISSION_CONFIG_FILE_LIST})

