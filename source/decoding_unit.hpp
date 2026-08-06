#pragma once

#include "codestream.hpp"

class Tile;
class Component;
class Resolution;
class Precinct;
class Subband;
class PrecinctSubband;
class CodeBlock;

class Tile {
private:
    static constexpr size_t MAIN_PACKET_BUFFER = 256;

    uint8_t main_packet_data[MAIN_PACKET_BUFFER];

public:
    Tile();
};
