###########################################################
#
# CAMERA_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the CAMERA_APP configuration
set(CAMERA_APP_PLATFORM_CONFIG_FILE_LIST
  camera_app_internal_cfg_values.h
  camera_app_platform_cfg.h
  camera_app_perfids.h
  camera_app_msgids.h
  camera_app_msgid_values.h
)

generate_configfile_set(${CAMERA_APP_PLATFORM_CONFIG_FILE_LIST})

