#include "decoding_unit.hpp"
#include "codestream.hpp"

j2k_Precinct::j2k_Precinct(const j2k_Component& cmp, const j2k_region<uint32_t>& rgn, uint32_t idx, uint8_t ns)
    : region{rgn}, index{idx}, num_subband{ns} {

    for ()
}

j2k_Subband::j2k_Subband() {
}

j2k_Resolution::j2k_Resolution(j2k_Tile& tile, const j2k_Component& cmp, const DFS& dfs, uint8_t nl)
    : precincts{tile.resource_ptr()}, resolution_level{nl}, df_direction{dfs.get_Ddfs(nl)} {
    uint8_t rx = resolution_level, ry = resolution_level;
    dfs.ceil_NL(rx, ry);
    const auto d     = pos2D{1u << rx, 1u << ry};
    const auto ppow2 = cmp.acs_psizes()[resolution_level].pow2();
    region.pos0      = ceil_int(cmp.get_region().pos0, d);
    region.pos1      = ceil_int(cmp.get_region().pos1, d);
    num_precinct     = ceil_int(region.pos1, ppow2) - region.pos0 / ppow2;

    uint8_t num_subband = (resolution_level != 0) ? 3 : 1;
    if (df_direction != 0 && df_direction & 0b10) num_subband = 1;

    {
        const pos2D ob[4]     = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
        const uint8_t gain[4] = {0, 1, 1, 2};
        const uint8_t nb      = j2kprf::NL + 1 - resolution_level - !resolution_level;
        const uint8_t b_pos   = !!resolution_level + df_direction & 0b01;
        for (uint8_t sb = 0; sb < num_subband; ++sb) {

            subbands.emplace_back();
        }
    }
    {
        assert(num_precinct.sum());
        const auto& precincts_size = cmp.acs_psizes();
        for (uint32_t i = 0; i < num_precinct.pro(); ++i) {
            const pos2D offset(region.pos0 / precincts_size[i].pow2());
            assert(offset.x == 0 && offset.y == 0);
            const pos2D xy(i % num_precinct.x, i / num_precinct.y);
            const pos2D p_pos0 = pos2D::max(region.pos0, precincts_size[i].pow2() * xy);
            const pos2D p_pos1 = pos2D::min(region.pos1, precincts_size[i].pow2() * (xy + 1));
            precincts.emplace_back(cmp, j2k_region<uint32_t>{p_pos0, p_pos1}, i, num_subband);
        }
    }
}

j2k_Component::j2k_Component(j2k_Tile& tile, uint8_t ci)
    : component_index{ci} {
    auto link_component = [&](const auto& vec_c) {
        for (const auto& c : vec_c) {
            if (c.get_component_index() == component_index) {
                return &c;
            }
        }
        return typename std::remove_reference_t<decltype(vec_c)>::const_pointer(nullptr);
    };
    const auto& mhd = tile.acs_main_header();
    const auto& rgn = tile.get_region();
    uint8_t dfs_idx;

    const auto& cod = mhd.cod;
    const auto& qcd = mhd.qcd;
    const COC* coc  = link_component(mhd.coc);
    const QCC* qcc  = link_component(mhd.qcc);

    region.pos0 = ceil_int(rgn.pos0, mhd.siz->get_Rsiz(component_index));
    region.pos1 = ceil_int(rgn.pos1, mhd.siz->get_Rsiz(component_index));

    auto write_coding_style = [&](const auto& c) {
        c->get_codeblock_size(codeblock_size);

        if (const auto NL = c->get_decomposition_level(); NL & 0x80) {
            dfs_idx = NL & 0x7F;
            // part 18 のプロファイルによって DFS マーカーの値は固定．
            if (dfs_idx == 1) {
                total_subband = 10;
            } else if (dfs_idx == 2) {
                total_subband = 12;
            } else {
                assert(false);
            }
        }

        uint8_t i = 0;
        for (auto& ps : precinct_sizes) {
            c->get_precinct_size(ps, i++);
        }
    };

    if (coc != nullptr) {
        write_coding_style(coc);
    } else {
        write_coding_style(cod);
    }

    auto write_quantization_style = [&](const auto& q) {
        quantization_style = q->get_quantization_style();
        num_guardbit       = q->get_number_of_guardbit();
        if (quantization_style != 1) {
            q_exponent.resize(total_subband);
            q_mantissa.resize(total_subband);
            for (uint8_t i = 0; i < total_subband; ++i) {
                q_exponent[i] = q->get_quantization_step_size_exponent(i);
                if (quantization_style == 2) {
                    q_mantissa[i] = q->get_quantization_step_size_mantissa(i);
                }
            }
        } else {
            q_exponent.push_back(q->get_quantization_step_size_exponent(0));
            q_mantissa.push_back(q->get_quantization_step_size_mantissa(0));
        }
    };

    if (qcc != nullptr) {
        write_quantization_style(qcc);
    } else {
        write_quantization_style(qcd);
    }
    for (uint8_t i = 0; i < resolutions.capacity(); ++i) {
        resolutions.emplace_back(tile, *this, mhd.dfs[dfs_idx], i);
    }
}

void j2k_Tile::init(const MainHeader& mhd, J2kBuf& buf) {
    this->main_header = &mhd;

    region.pos0 = {0, 0};
    region.pos1 = pos2D::min(mhd.siz->get_Tsiz(), mhd.siz->get_Siz());

    for (size_t i = 0; i < mhd.siz->get_Csiz(); ++i) {
        tile_components.emplace_back(*this, i);
    }
}
