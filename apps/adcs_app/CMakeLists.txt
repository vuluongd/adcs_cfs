project(CFE_ADCS_APP C)

set(APP_SRC_FILES
  fsw/src/adcs_app.c
  fsw/src/adcs_app_cmds.c
  fsw/src/adcs_app_utils.c
  fsw/src/adcs_app_control.c
  fsw/src/adcs_app_imu.c
  fsw/src/adcs_app_light.c
  fsw/src/adcs_app_loop.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/adcs_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/adcs_app_dispatch.c
  )
endif()

# Create the app module
add_cfe_app(adcs_app ${APP_SRC_FILES})

target_include_directories(adcs_app PUBLIC fsw/inc)
target_include_directories(adcs_app PUBLIC ../light_app/fsw/inc)
target_include_directories(adcs_app PUBLIC ../imu_app/fsw/inc)
target_include_directories(adcs_app PUBLIC ../act_app/fsw/inc)

# Include the public API from sample_lib
add_cfe_app_dependency(adcs_app sample_lib)

# Add table
add_cfe_tables(adcs_app fsw/tables/adcs_app_tbl.c)

#if (ENABLE_UNIT_TESTS)
  #add_subdirectory(unit-test)
#endif (ENABLE_UNIT_TESTS)
