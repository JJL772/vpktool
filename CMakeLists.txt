cmake_minimum_required(VERSION 3.5)
project(vpktool)
project(libvpk)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBVPK_SRCS
        src/vpk.cpp)

set(VPKTOOL_SRCS src/vpktool.cpp)

add_library(libvpk STATIC ${LIBVPK_SRCS})

include_directories(thirdparty)

add_executable(vpktool ${VPKTOOL_SRCS})
target_link_libraries(vpktool libvpk)

if(UNIX AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -g")
endif()
set_target_properties(libvpk PROPERTIES CXX_STANDARD 20)
set_target_properties(vpktool PROPERTIES CXX_STANDARD 20)
