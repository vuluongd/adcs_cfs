project(CFE_ACT_APP C)

if(CFE_EDS_ENABLED)
  message(FATAL_ERROR "ACT_APP WHEEL_DATA integration currently requires CFE_EDS_ENABLED=OFF")
endif()

# Motor layer has no cFS dependency: built as its own library so tools/act_demo can test it on a PC.
add_library(act_motor_core STATIC fsw/src/act_motor.c)
set_target_properties(act_motor_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(act_motor_core PUBLIC fsw/inc fsw/src)
target_link_libraries(act_motor_core PUBLIC m)

set(APP_SRC_FILES
  fsw/src/act_app.c
  fsw/src/act_app_cmds.c
  fsw/src/act_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/act_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/act_app_dispatch.c
  )
endif()

# Create the app module
add_cfe_app(act_app ${APP_SRC_FILES} fsw/src/act_app_wheel.c)
target_link_libraries(act_app act_motor_core)

target_include_directories(act_app PUBLIC fsw/inc)
# Include the public API from sample_lib to demonstrate how
# to call library-provided functions
add_cfe_app_dependency(act_app sample_lib)

# Add table
add_cfe_tables(act_app fsw/tables/act_app_tbl.c)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)
