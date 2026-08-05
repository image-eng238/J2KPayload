cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 17)

project(RTP_payload CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# global defines

# add_definitions(-DGENERATE_LOG)
# add_definitions(-DGENERATE_RECEIVE_PROBABILITY)
# add_definitions(-DRTP_CLOCK_CHECK)
# add_definitions(-DDISABLE_TABLE)

add_definitions(-DENABLE_EXPECT)


# build flags

#compile options コンパイラは GCC を想定

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wsign-conversion -Wunused-parameter -Wformat=0 -fexceptions -fdiagnostics-color=always")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address")
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=memory")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -g -fomit-frame-pointer -finline-functions -finline-limit-20000 -finline -funroll-loops -flto=auto")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -fsanitize=address")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -fno-default-inline -fomit-frame-pointer ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -UNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g -fomit-frame-pointer -finline-functions -finline-limit-20000 -finline -funroll-loops -flto=auto -gdwarf-5")

# sub directories
add_subdirectory(./source)
include_directories(./source)

# executes
add_executable(j2kpay main.cpp)
target_link_libraries(j2kpay CodeStream Decoding LeakyBucket RTP_header)

add_executable(filep file_parse.cpp)
target_compile_definitions(Decoding PUBLIC -DGENERATE_LOG)
target_link_libraries(filep CodeStream Decoding LeakyBucket RTP_header)

add_executable(RTPsender RTPsender.cpp)
add_executable(RTPsender2 RTPsender2.cpp)

add_executable(stop_j2kpay stop_j2kpay.cpp)

add_executable(sender_test sender_test.cpp)

add_executable(UDPtest UDPtest.cpp)

add_executable(RTP_show RTP_show.cpp)


# test

add_subdirectory(./tests)