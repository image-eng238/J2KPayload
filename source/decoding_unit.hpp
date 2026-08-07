#pragma once

#include "codestream.hpp"
#include "fixed_capacity_vector.hpp"
#include "mem_resource.hpp"
#include "packet_t.hpp"

class j2k_Tile;
class Component;
class Resolution;
class Subband;
class Precinct;
class PrecinctSubband;
class CodeBlock;

class CodeBlock {};
class PrecinctSubband {};
class Precinct {};
class Subband {};
class Resolution {};
class Component {};

class j2k_Tile {
private:
    // static constexpr size_t MAIN_PACKET_BUFFER = 256;
    // uint8_t main_packet_data[MAIN_PACKET_BUFFER];

    j2k_resource<Precinct, PrecinctSubband, CodeBlock> heap_mem;
    const MainHeader* main_header;
    fixed_capacity_vector<Component, j2kprf::Csiz_max> tile_components;
    std::array<pos2D, j2kprf::NL + 1> precinct_sizes;

public:
    j2k_Tile();
};
