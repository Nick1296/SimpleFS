cmake_minimum_required(VERSION 3.0)
#we set the project name
project(SimpleFS)
# we set the source files to be used
set(SOURCES_NO_PERMS common.h bitmap.c disk_driver.c simplefs.c simplefs_aux.c simplefs_structures.h disk_driver_structures.h bitmap_structures.h)
set(SOURCES common.h bitmap.c disk_driver.c simplefs.c simplefs_aux.c simplefs_apis.c users.c simplefs_structures.h disk_driver_structures.h bitmap_structures.h)
#we add the executable targets to be built
add_executable(SimpleFS_shell ${SOURCES} shell.c)
add_executable(SimpleFS_test ${SOURCES_NO_PERMS} simplefs_test.c)
add_executable(SimpleFS_apis_test ${SOURCES} simplefs_apis_test.c)
add_executable(SimpleFS_users_test ${SOURCES} users_test.c)
#we set the prpeties for these executables (for now only the output directory)
set_target_properties(SimpleFS_shell PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
set_target_properties(SimpleFS_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
set_target_properties(SimpleFS_apis_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
set_target_properties(SimpleFS_users_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
# we set the debug compilation flags to be used
set(CMAKE_C_FLAGS_DEBUG "-fsanitize=address -Wall -ggdb -std=gnu99 -Wstrict-prototypes")