#include "decoding_unit.hpp"

j2k_Component::j2k_Component(const MainHeader& mhd, j2k_region<uint32_t> rgn, uint8_t ci)
    : component_index{ci} {
    auto link_component = [&](auto vec_c) {
        for (const auto& c : vec_c) {
            if (c.get_component_index() == component_index) {
                return &c;
            }
        }
        return typename decltype(vec_c)::const_pointer(nullptr);
    };

    const auto& cod = mhd.cod;
    const auto& qcd = mhd.qcd;
    const COC* coc  = link_component(mhd.coc);
    const QCC* qcc  = link_component(mhd.qcc);

    region.pos0 = ceil_int(rgn.pos0, mhd.siz->get_Rsiz(component_index));
    region.pos0 = ceil_int(rgn.pos1, mhd.siz->get_Rsiz(component_index));

    auto write_coding_style = [&](const auto& c) {
        c->get_codeblock_size(codeblock_size);

        if (const auto NL = c->get_decomposition_level(); NL & 0x80) {
            const uint8_t dfs_idx = NL & 0x7F;
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

    auto write_quantization_style = [&](auto q) {
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
}

void j2k_Tile::init(const MainHeader& mhd, J2kBuf& buf) {
    this->main_header = &mhd;

    region.pos0 = {0, 0};
    region.pos1 = pos2D::min(mhd.siz->get_Tsiz(), mhd.siz->get_Siz());

    for (size_t i = 0; i < mhd.siz->get_Csiz(); ++i) {
        tile_components.emplace_back(mhd, region, i);
    }
}
