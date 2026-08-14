cmake_minimum_required(VERSION 3.5.0)

add_executable(test_mem_resource ./src/mem_resource.cpp)
add_executable(test_fixed_capacity_vector ./src/fixed_capacity_vector.cpp)
add_executable(test_packet_t ./src/packet_t.cpp)

add_executable(test_decoding_unit ./src/decoding_unit.cpp)
target_link_libraries(test_decoding_unit CodeStream Decoding_unit LeakyBucket RTP_header)
