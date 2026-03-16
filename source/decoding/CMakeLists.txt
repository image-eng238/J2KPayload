cmake_minimum_required(VERSION 3.5.0)

include_directories(../)

add_library(Decoding STATIC tile.cpp other.cpp tagtree.cpp)
# target_compile_definitions(Decoding PRIVATE USE_MULTIMEM)