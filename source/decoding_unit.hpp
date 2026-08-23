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

class j2k_CodeBlock {
private:
    j2k_region<uint32_t> region;
    uint8_t* codeblock_data;
    uint32_t length;
    uint8_t number_of_zbp;
    uint8_t band_pos;

public:
    j2k_CodeBlock() = default;
    j2k_CodeBlock(const j2k_region<uint32_t> rgn, uint8_t spos);
};

class j2k_PrecinctSubband {
private:
    j2k_CodeBlock codeblock;

public:
    j2k_PrecinctSubband() = default;
    j2k_PrecinctSubband(j2k_CodeBlock&& cb) : codeblock{cb} {}
    auto& acs_codeblock() { return codeblock; }
    const auto& acs_codeblock() const { return codeblock; }
};

class j2k_Precinct {
private:
    // fixed_capacity_vector<j2k_PrecinctSubband, 3> pband;
    std::pmr::vector<j2k_PrecinctSubband> pband;
    j2k_region<uint32_t> region;
    uint32_t PID;

public:
    j2k_Precinct(j2k_Tile& tile, const j2k_Component& cmp, const j2k_region<uint32_t>& rgn, uint8_t ns, uint32_t PID = 0);
    const auto& acs_pband() const { return pband; }
    const auto& get_region() const { return region; }
    auto get_PID() const { return PID; }
};

class j2k_Subband {
private:
    const j2k_Component* parent_component;
    const j2k_Resolution* parent_resolution;
    j2k_region<uint32_t> region;
    uint8_t position;

public:
    j2k_Subband(const j2k_Component& cmp, const j2k_Resolution& rsl, const j2k_region<uint32_t>& rgn, uint8_t pos);

    uint8_t get_position() const { return position; }
};

class j2k_Resolution {
private:
    fixed_capacity_vector<j2k_Subband, 3> subbands;
    std::pmr::vector<j2k_Precinct> precincts;
    pos2D num_precinct;
    j2k_region<uint32_t> region;
    uint8_t resolution_level;
    uint8_t df_direction;
    Postion2D<uint8_t> resolution_xy;

public:
    j2k_Resolution(j2k_Tile& tile, const j2k_Component& cmp, const DFS& dfs, uint8_t nl);
    void construct_precincts(j2k_Tile& tile, const j2k_Component& cmp);
    const auto& acs_precincts() const { return precincts; }
    const auto& acs_subbands() const { return subbands; }
    const auto& get_region() const { return region; }
    const auto& get_num_precinct() const { return num_precinct; }
    uint8_t get_resolution_level() const { return resolution_level; }
    uint8_t get_df_direction() const { return df_direction; }
    const auto& get_resolution_xy() const { return resolution_xy; }
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
    j2k_Component(j2k_Tile& tile, uint8_t ci);
    auto& acs_psizes() { return precinct_sizes; }
    const auto& acs_psizes() const { return precinct_sizes; }
    const auto& get_region() const { return region; }
    auto& acs_resolutions() { return resolutions; }
    const auto& acs_resolutions() const { return resolutions; }
    const auto& get_codeblock_size() const { return codeblock_size; }
    uint8_t get_index() const { return component_index; }
};

class j2k_Tile {
public:
    using resource_t = j2k_parent_resource<j2k_Precinct, j2k_PrecinctSubband>;

private:
    resource_t heap_resources;
    const MainHeader* main_header;
    fixed_capacity_vector<j2k_Component, j2kprf::Csiz_max> tile_components;

    j2k_region<uint32_t> region;
    std::pmr::vector<j2k_Precinct> table;
    size_t total_precinct;
    size_t total_pband;

public:
    j2k_Tile() : heap_resources{}, main_header{}, tile_components{}, region{}, table{resource_ptr<0>()}, total_precinct{}, total_pband{} {}
    void move_resource(const resource_t& resource) = delete;
    void move_resource(resource_t&& resource) { heap_resources = std::move(resource); }
    template <size_t Index>
    j2k_child_resource* resource_ptr() { return heap_resources.get_resource<Index>(); }
    auto& acs_main_header() { return *main_header; }
    const auto& acs_main_header() const { return *main_header; }
    const auto& get_region() const { return region; }
    void init(const MainHeader& mhd, J2kBuf& buf);
    void add_total_precinct(uint32_t n) { total_precinct += n; }
    void add_total_pband(uint32_t n) { total_pband += n; }
    void build_table();
};
