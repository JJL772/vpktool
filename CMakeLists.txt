cmake_minimum_required(VERSION 3.5)
project(vpktool)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VPKTOOL_SRCS src/vpktool.cpp)

include_directories(thirdparty)

add_executable(vpktool ${VPKTOOL_SRCS})

if(UNIX AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -g")
endif()
set_target_properties(vpktool PROPERTIES CXX_STANDARD 20)
