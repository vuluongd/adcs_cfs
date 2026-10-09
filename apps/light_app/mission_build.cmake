###########################################################
#
# LIGHT_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the LIGHT_APP configuration
set(LIGHT_APP_MISSION_CONFIG_FILE_LIST
  light_app_fcncode_values.h
  light_app_interface_cfg_values.h
  light_app_mission_cfg.h
  light_app_perfids.h
  light_app_msg.h
  light_app_msgdefs.h
  light_app_msgstruct.h
  light_app_tbl.h
  light_app_tbldefs.h
  light_app_tblstruct.h
  light_app_topicid_values.h
)

generate_configfile_set(${LIGHT_APP_MISSION_CONFIG_FILE_LIST})

