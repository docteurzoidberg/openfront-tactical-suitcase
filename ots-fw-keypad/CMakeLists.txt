cmake_minimum_required(VERSION 3.16)

# Add shared components from ots-fw-shared
list(APPEND EXTRA_COMPONENT_DIRS "../ots-fw-shared/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ots-fw-keypad)
