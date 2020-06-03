cmake_minimum_required(VERSION 3.5)

project(vpktool C CXX)

set(SRCS vpktool.cpp vpk1.cpp vpk2.cpp vpk.cpp vpk.h)

add_executable(vpktool ${SRCS})
set_target_properties(vpktool PROPERTIES CXX_STANDARD 17)