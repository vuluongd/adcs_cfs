###########################################################
#
# ACT_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the ACT_APP configuration
set(ACT_APP_MISSION_CONFIG_FILE_LIST
  act_app_fcncode_values.h
  act_app_interface_cfg_values.h
  act_app_mission_cfg.h
  act_app_perfids.h
  act_app_msg.h
  act_app_msgdefs.h
  act_app_msgstruct.h
  act_app_tbl.h
  act_app_tbldefs.h
  act_app_tblstruct.h
  act_app_topicid_values.h
)

generate_configfile_set(${ACT_APP_MISSION_CONFIG_FILE_LIST})

