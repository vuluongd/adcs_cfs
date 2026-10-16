project(CFE_LIGHT_APP C)

if(CFE_EDS_ENABLED)
  message(FATAL_ERROR "BH1750 LIGHT_DATA integration currently requires CFE_EDS_ENABLED=OFF")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "BH1750 driver currently targets Linux; port light_sensor.c for this OS")
endif()

set(APP_SRC_FILES
  fsw/src/light_app.c
  fsw/src/light_app_cmds.c
  fsw/src/light_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/light_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/light_app_dispatch.c
  )
endif()

# Create the app module
add_library(light_tracking_core STATIC
  fsw/src/light_tracking.c fsw/src/light_sensor.c
  fsw/src/light_angle.c fsw/src/light_sim.c fsw/src/light_pins.c
)
set_target_properties(light_tracking_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(light_tracking_core PUBLIC fsw/inc fsw/src)
target_link_libraries(light_tracking_core PUBLIC m)
add_cfe_app(light_app ${APP_SRC_FILES} fsw/src/light_app_tracking.c)
target_link_libraries(light_app light_tracking_core)

target_include_directories(light_app PUBLIC fsw/inc)
# Include the public API from sample_lib to demonstrate how
# to call library-provided functions
add_cfe_app_dependency(light_app sample_lib)

# Add table
add_cfe_tables(light_app fsw/tables/light_app_tbl.c)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  # add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)
