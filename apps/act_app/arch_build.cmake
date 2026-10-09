###########################################################
#
# ACT_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the ACT_APP configuration
set(ACT_APP_PLATFORM_CONFIG_FILE_LIST
  act_app_internal_cfg_values.h
  act_app_platform_cfg.h
  act_app_perfids.h
  act_app_msgids.h
  act_app_msgid_values.h
)

generate_configfile_set(${ACT_APP_PLATFORM_CONFIG_FILE_LIST})

