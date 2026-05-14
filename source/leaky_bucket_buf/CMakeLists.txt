cmake_minimum_required(VERSION 3.5.0)

include_directories(../)

add_library(LeakyBucket STATIC buf.cpp access.cpp)