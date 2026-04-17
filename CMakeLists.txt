cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 17)

project(RTP_payload CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# global defines

# add_definitions(-DGENERATE_LOG)

# sub directories
add_subdirectory(./source)
include_directories(./source)
include_directories(/home/olab/Progs/Take_Library/)

# build flags

#compile options コンパイラは GCC を想定

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wsign-conversion -Wunused-parameter -Wformat=0 -fexceptions -fdiagnostics-color=always")

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address")
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=memory")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -fno-default-inline -fomit-frame-pointer")

# executes
add_executable(j2kpay main.cpp)
target_link_libraries(j2kpay CodeStream Decoding LeakyBucket)

add_executable(RTPsender RTPsender.cpp)

add_executable(UDPtest UDPtest.cpp)