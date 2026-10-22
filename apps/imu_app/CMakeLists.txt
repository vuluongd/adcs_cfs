project(CFE_IMU_APP C CXX)

if(CFE_EDS_ENABLED)
  message(FATAL_ERROR "MPU6050 IMU_DATA integration currently requires CFE_EDS_ENABLED=OFF")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "MPU6050 driver currently targets Linux (i2c-dev); port imu_sensor.cpp for this OS")
endif()

# C++ is used for the gyro core and its cFS glue (fsw/src/*.cpp). The NASA template glue stays C.
# imu_app_processing.cpp is deliberately NOT in APP_SRC_FILES: unit-test/ builds one coverage test per entry.
set(APP_SRC_FILES
  fsw/src/imu_app.c
  fsw/src/imu_app_cmds.c
  fsw/src/imu_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/imu_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/imu_app_dispatch.c
  )
endif()

# C++ filter + sensor core (no cFS dependency, unit-testable on a PC with tests/test_imu_filter.cpp)
add_library(imu_filter_core STATIC fsw/src/imu_filter.cpp fsw/src/imu_sensor.cpp)
set_target_properties(imu_filter_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)
target_compile_options(imu_filter_core PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_include_directories(imu_filter_core PUBLIC fsw/inc fsw/src)
target_link_libraries(imu_filter_core PUBLIC m)

# Create the app module (the glue .cpp needs the cFE include paths, so it is part of the app target)
add_cfe_app(imu_app ${APP_SRC_FILES} fsw/src/imu_app_processing.cpp)
set_target_properties(imu_app PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(imu_app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_link_libraries(imu_app imu_filter_core)

target_include_directories(imu_app PUBLIC fsw/inc)
# Include the public API from sample_lib to demonstrate how
# to call library-provided functions
add_cfe_app_dependency(imu_app sample_lib)

# Add table
add_cfe_tables(imu_app fsw/tables/imu_app_tbl.c)

# If UT is enabled, then add the tests from the subdirectory
# Note that this is an app, and therefore does not provide
# stub functions, as other entities would not typically make
# direct function calls into this application.
if (ENABLE_UNIT_TESTS)
  add_subdirectory(unit-test)
endif (ENABLE_UNIT_TESTS)
