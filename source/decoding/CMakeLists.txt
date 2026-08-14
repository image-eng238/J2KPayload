cmake_minimum_required(VERSION 3.5.0)

include_directories(../)

add_library(Decoding STATIC tile.cpp other.cpp tagtree.cpp)
target_link_libraries(Decoding PUBLIC CodeStream)

add_library(Decoding_unit STATIC decoding_unit.cpp)
target_link_libraries(Decoding_unit PUBLIC CodeStream)
