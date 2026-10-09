###########################################################
#
# ADCS_APP platform build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# The list of header files that control the ADCS_APP configuration
set(ADCS_APP_PLATFORM_CONFIG_FILE_LIST
  adcs_app_internal_cfg_values.h
  adcs_app_platform_cfg.h
  adcs_app_perfids.h
  adcs_app_msgids.h
  adcs_app_msgid_values.h
)

generate_configfile_set(${ADCS_APP_PLATFORM_CONFIG_FILE_LIST})

