# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Exclude test tone and large test WAV to save memory
list(FILTER app_sources EXCLUDE REGEX ".*test_tone_440hz_wav\\.c$")
list(FILTER app_sources EXCLUDE REGEX ".*audio_test_tone\\.c$")

idf_component_register(SRCS ${app_sources})
