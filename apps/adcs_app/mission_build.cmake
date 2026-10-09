###########################################################
#
# ADCS_APP mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the ADCS_APP configuration
set(ADCS_APP_MISSION_CONFIG_FILE_LIST
  adcs_app_fcncode_values.h
  adcs_app_interface_cfg_values.h
  adcs_app_mission_cfg.h
  adcs_app_perfids.h
  adcs_app_msg.h
  adcs_app_msgdefs.h
  adcs_app_msgstruct.h
  adcs_app_tbl.h
  adcs_app_tbldefs.h
  adcs_app_tblstruct.h
  adcs_app_topicid_values.h
)

generate_configfile_set(${ADCS_APP_MISSION_CONFIG_FILE_LIST})

