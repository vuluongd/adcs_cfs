##################################################################
#
# Coverage Unit Test build recipe
#
# This CMake file contains the recipe for building the sample unit tests.
# It is invoked from the parent directory when unit tests are enabled.
#
##################################################################

#
# NOTE on the subdirectory structures here:
#
# - "inc" provides local header files shared between the coveragetest,
#    wrappers, and overrides source code units
# - "coveragetest" contains source code for the actual unit test cases
#    The primary objective is to get line/path coverage on the FSW
#    code units.
#

# Use the UT assert public API, and allow direct
# inclusion of source files that are normally private
include_directories(../fsw/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_cfe_coverage_stubs(adcs_app
  stubs/adcs_app_global_stubs.c
  stubs/adcs_app_stubs.c
  stubs/adcs_app_cmds_stubs.c
  stubs/adcs_app_dispatch_stubs.c
  stubs/adcs_app_utils_stubs.c
)

add_library(adcs_app_ut_common STATIC
    common/eventcheck.c
    common/setup.c
)

target_include_directories(adcs_app_ut_common PUBLIC common)
target_link_libraries(adcs_app_ut_common core_api ut_assert)


# Generate a dedicated "testrunner" executable for each test file
# Accomplish this by cycling through all the app's source files, there must be
# a *_tests file for each
# Chỉ tạo coverage test cho những module đã có file coveragetest_*.c
set(UT_SRC_FILES
  fsw/src/adcs_app.c
  fsw/src/adcs_app_cmds.c
  fsw/src/adcs_app_utils.c
  fsw/src/adcs_app_dispatch.c
)

foreach(SRCFILE ${UT_SRC_FILES})

    get_filename_component(UNIT_NAME "${SRCFILE}" NAME_WE)
    set(TESTS_NAME "coveragetest_${UNIT_NAME}")
    set(TESTS_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/coveragetest/${TESTS_NAME}.c")

    add_cfe_coverage_test(adcs_app "${UNIT_NAME}" "${TESTS_SOURCE_FILE}" "../${SRCFILE}")
    add_cfe_coverage_dependency(adcs_app "${UNIT_NAME}" adcs_app)
    target_link_libraries(coverage-adcs_app-${UNIT_NAME}-testrunner coverage-adcs_app-stubs adcs_app_ut_common)

endforeach()



# The adcs_app uses library functions provided by sample_lib so must be linked
# with the sample_lib stub library (this is mainly just an example of how this
# can be done).
add_cfe_coverage_dependency(adcs_app adcs_app_cmds sample_lib)
