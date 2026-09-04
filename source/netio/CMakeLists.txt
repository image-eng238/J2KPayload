cmake_minimum_required(VERSION 3.5.0)

include_directories(../)

add_library(UDP STATIC UDP.cpp)
add_library(RTP_header STATIC RTP_header.cpp)
target_link_libraries(RTP_header PUBLIC LeakyBucket UDP)