cmake_minimum_required(VERSION 3.16)
project(act_demo C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror -pedantic)

set(ACT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../apps/act_app")
add_library(act_core STATIC "${ACT_DIR}/fsw/src/act_motor.c")
target_include_directories(act_core PUBLIC "${ACT_DIR}/fsw/inc" "${ACT_DIR}/fsw/src")
target_link_libraries(act_core PUBLIC m)

add_executable(act_demo main.c)
target_link_libraries(act_demo PRIVATE act_core)

enable_testing()
add_executable(test_motor "${ACT_DIR}/tests/test_motor.c")
target_link_libraries(test_motor PRIVATE act_core)
add_test(NAME slew_clamp_reverse_stop COMMAND test_motor)
set_tests_properties(slew_clamp_reverse_stop PROPERTIES ENVIRONMENT "ACT_APP_BACKEND=sim")
