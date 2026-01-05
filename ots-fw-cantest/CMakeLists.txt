# CMakeLists.txt for ots-fw-cantest
cmake_minimum_required(VERSION 3.16)

# Add shared components directory
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../ots-fw-shared/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ots-fw-cantest)
