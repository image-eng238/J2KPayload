cmake_minimum_required(VERSION 3.5.0)

include_directories(../)

add_library(CodeStream STATIC j2kbuf.cpp marker.cpp)