# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Exclude old serial_commands system (replaced by audio_console)
list(FILTER app_sources EXCLUDE REGEX ".*serial_commands\\.c$")

# Always include audio_console (unified interface)
message(STATUS "Audio console: ENABLED (unified command interface)")

idf_component_register(SRCS ${app_sources})
