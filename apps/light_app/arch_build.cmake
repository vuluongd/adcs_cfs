###########################################################
#
# LIGHT_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the LIGHT_APP configuration
set(LIGHT_APP_PLATFORM_CONFIG_FILE_LIST
  light_app_internal_cfg_values.h
  light_app_platform_cfg.h
  light_app_perfids.h
  light_app_msgids.h
  light_app_msgid_values.h
)

generate_configfile_set(${LIGHT_APP_PLATFORM_CONFIG_FILE_LIST})

