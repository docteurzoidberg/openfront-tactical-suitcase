cmake_minimum_required(VERSION 3.16.0)

# Add shared components path
list(APPEND EXTRA_COMPONENT_DIRS "../ots-fw-shared/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ots-fw-audiomodule)
