cmake_minimum_required(VERSION 3.5)
project(vpktool)
project(libvpk)

set(LIBVPK_SRCS src/vpk1.cpp src/vpk2.cpp src/wad.cpp src/vpk.cpp)
set(VPKTOOL_SRCS src/vpktool.cpp)

add_library(libvpk STATIC ${LIBVPK_SRCS})

add_executable(vpktool ${VPKTOOL_SRCS})
target_link_libraries(vpktool libvpk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -g")

set_target_properties(libvpk PROPERTIES CXX_STANDARD 17)
set_target_properties(vpktool PROPERTIES CXX_STANDARD 17)