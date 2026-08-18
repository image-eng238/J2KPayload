#pragma once

#include "codestream.hpp"
#include "fixed_capacity_vector.hpp"
#include "mem_resource.hpp"
#include "packet_t.hpp"

#include <vector>
#include <utility>

template <typename T = uint32_t>
struct j2k_region {
    Postion2D<T> pos0;
    Postion2D<T> pos1;
};

class j2k_Tile;
class j2k_Component;
class j2k_Resolution;
class j2k_Subband;
class j2k_Precinct;
class j2k_PrecinctSubband;
class j2k_CodeBlock;

class j2k_CodeBlock {};
class j2k_PrecinctSubband {};

class j2k_Precinct {
    fixed_capacity_vector<j2k_PrecinctSubband, 3> pband;
    j2k_region<uint32_t> region;
    uint32_t index;

    uint8_t num_subband;

public:
    j2k_Precinct(const j2k_Component& cmp, const j2k_region<uint32_t>& rgn, uint32_t idx, uint8_t ns);
};

class j2k_Subband {
    j2k_region<uint32_t> region;
    j2k_Component* parent_component;
    j2k_Resolution* parent_resolution;
    uint8_t position;

public:
    j2k_Subband();
};

class j2k_Resolution {
private:
    fixed_capacity_vector<j2k_Subband, 3> subbands;
    std::pmr::vector<j2k_Precinct> precincts;
    pos2D num_precinct;
    j2k_region<uint32_t> region;
    uint8_t resolution_level;
    uint8_t df_direction;

public:
    j2k_Resolution(j2k_Tile& tile, const j2k_Component& cmp, const DFS& dfs, uint8_t nl);
    const auto& get_region() const { return region; }
    uint8_t get_resolution_level() const { return resolution_level; }
    uint8_t get_df_direction() const { return df_direction; }
};

class j2k_Component {
private:
    std::array<pos2D, j2kprf::NL + 1> precinct_sizes;
    fixed_capacity_vector<j2k_Resolution, j2kprf::NL + 1> resolutions;
    pos2D codeblock_size;
    j2k_region<uint32_t> region;
    fixed_capacity_vector<uint16_t, j2kprf::Subband_max> q_mantissa;
    fixed_capacity_vector<uint8_t, j2kprf::Subband_max> q_exponent;
    uint8_t component_index;
    uint8_t total_subband;
    uint8_t quantization_style;
    uint8_t num_guardbit;

public:
    j2k_Component(j2k_Tile& mhd, uint8_t ci);
    auto& acs_psizes() { return precinct_sizes; }
    const auto& acs_psizes() const { return precinct_sizes; }
    const auto& get_region() const { return region; }
    const auto& get_resolutions() const { return resolutions; }
};

class j2k_Tile {
private:
    // static constexpr size_t MAIN_PACKET_BUFFER = 256;
    // uint8_t main_packet_data[MAIN_PACKET_BUFFER];

    j2k_resource<j2k_Precinct, j2k_PrecinctSubband, j2k_CodeBlock> heap_resources;
    const MainHeader* main_header;
    fixed_capacity_vector<j2k_Component, j2kprf::Csiz_max> tile_components;

    j2k_region<uint32_t> region;

public:
    void move_resource(const decltype(heap_resources)& resource) = delete;
    void move_resource(decltype(heap_resources)&& resource) { heap_resources = std::move(resource); }
    auto resource_ptr() { return &heap_resources; }
    auto& acs_main_header() { return *main_header; }
    const auto& acs_main_header() const { return *main_header; }
    const auto& get_region() const { return region; }
    void init(const MainHeader& mhd, J2kBuf& buf);
    void build_table() const;
};
