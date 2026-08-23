#include "decoding_unit.hpp"
#include "codestream.hpp"
#include <numeric>
#include <vector>

j2k_CodeBlock::j2k_CodeBlock(const j2k_region<uint32_t> rgn, uint8_t spos)
    : region{rgn}, codeblock_data{}, length{}, number_of_zbp{}, band_pos{spos} {}

j2k_Precinct::j2k_Precinct(j2k_Tile& tile, const j2k_Component& cmp, const j2k_region<uint32_t>& rgn, uint8_t ns, uint32_t PID)
    : pband{tile.resource_ptr<1>()}, region{rgn}, PID{PID} {

    const pos2D ob[4] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    pband.reserve(ns);
    for (uint8_t i = 0; i < ns; ++i) {
        const uint8_t spos = 0;
        const uint8_t sr   = (spos == 0) ? 1 << 0 : 1 << 1;

        const pos2D pb_pos0 = ceil_int(region.pos0 - ob[spos], sr);
        const pos2D pb_pos1 = ceil_int(region.pos1 - ob[spos], sr);
        pband.emplace_back(j2k_CodeBlock{j2k_region<uint32_t>{pb_pos0, pb_pos1}, spos});
    }
}

j2k_Subband::j2k_Subband(const j2k_Component& cmp, const j2k_Resolution& rsl, const j2k_region<uint32_t>& rgn, uint8_t pos)
    : parent_component{&cmp}, parent_resolution{&rsl}, region{rgn}, position{pos} {
}

j2k_Resolution::j2k_Resolution(j2k_Tile& tile, const j2k_Component& cmp, const DFS& dfs, uint8_t nl)
    : precincts{tile.resource_ptr<0>()}, resolution_level{nl}, df_direction{dfs.get_Ddfs(nl)}, resolution_xy{resolution_level} {
    dfs.ceil_NL(resolution_xy.x, resolution_xy.y);
    const auto d     = pos2D{1u << j2kprf::NL - resolution_xy.x, 1u << j2kprf::NL - resolution_xy.y};
    const auto ppow2 = cmp.acs_psizes()[resolution_level].pow2();
    region.pos0      = ceil_int(cmp.get_region().pos0, d);
    region.pos1      = ceil_int(cmp.get_region().pos1, d);
    num_precinct     = ceil_int(region.pos1, ppow2) - region.pos0 / ppow2;
    tile.add_total_precinct(num_precinct.pro());

    uint8_t num_subband = (resolution_level != 0) ? 3 : 1;
    if (df_direction != 0 && df_direction & 0b10) num_subband = 1;

    const pos2D ob[4]     = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    const uint8_t gain[4] = {0, 1, 1, 2};
    const uint8_t nb      = j2kprf::NL + 1 - resolution_level - !resolution_level;
    uint8_t b_pos         = !!resolution_level + df_direction & 0b01;
    for (uint8_t sb = 0; sb < num_subband; ++sb) {
        const pos2D sb_pos0 = ceil_int(cmp.get_region().pos0 - (ob[b_pos] * (1 << (nb - 1))), 1 << nb);
        const pos2D sb_pos1 = ceil_int(cmp.get_region().pos1 - (ob[b_pos] * (1 << (nb - 1))), 1 << nb);
        subbands.emplace_back(cmp, *this, j2k_region<uint32_t>{sb_pos0, sb_pos1}, b_pos++);
    }
    tile.add_total_pband(num_subband * num_precinct.pro());

    assert(num_precinct.sum());
    // construct_precincts(tile, cmp);
}

void j2k_Resolution::construct_precincts(j2k_Tile& tile, const j2k_Component& cmp) {
    const auto& psiz = cmp.acs_psizes()[resolution_level];
    precincts.reserve(num_precinct.pro());
    for (uint32_t i = 0; i < num_precinct.pro(); ++i) {
        const pos2D offset(region.pos0 / psiz.pow2());
        assert(offset.x == 0 && offset.y == 0);
        const pos2D xy(i % num_precinct.x, i / num_precinct.x);
        const pos2D p_pos0 = pos2D::max(region.pos0, psiz.pow2() * xy);
        const pos2D p_pos1 = pos2D::min(region.pos1, psiz.pow2() * (xy + 1));
        precincts.emplace_back(tile, cmp, j2k_region<uint32_t>{p_pos0, p_pos1}, static_cast<uint8_t>(subbands.size()));
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
        resolutions.emplace_back(tile, *this, mhd.dfs[dfs_idx - 1], i);
    }
}

void j2k_Tile::init(const MainHeader& mhd, J2kBuf& buf) {
    this->main_header = &mhd;

    region.pos0 = {0, 0};
    region.pos1 = pos2D::min(mhd.siz->get_Tsiz(), mhd.siz->get_Siz());

    for (size_t i = 0; i < mhd.siz->get_Csiz(); ++i) {
        tile_components.emplace_back(*this, i);
    }
    build_table();
}

void j2k_Tile::build_table() {
    if (!heap_resources.is_allocated()) {
        auto t    = std::chrono::high_resolution_clock::now();
        auto size = heap_resources.prev_allocate(total_precinct, total_pband);
        auto r    = std::chrono::high_resolution_clock::now() - t;
        std::cout << "alloc time(ns) = " << std::chrono::duration_cast<std::chrono::nanoseconds>(r).count()
                  << ", size(byte) = " << size << std::endl;
    }
    table.reserve(total_precinct);

    const auto progression_order = main_header->cod->get_progression_order();
    const pos2D PPxy{
        std::accumulate(
            tile_components.begin(), tile_components.end(), static_cast<uint32_t>(1),
            [](uint32_t acc, const j2k_Component& val) { return std::lcm(acc, val.get_codeblock_size().x); }
        ),
        std::accumulate(
            tile_components.begin(), tile_components.end(), static_cast<uint32_t>(1),
            [](uint32_t acc, const j2k_Component& val) { return std::lcm(acc, val.get_codeblock_size().y); }
        )
    };
    const auto& siz      = main_header->siz;
    const uint32_t XOsiz = siz->get_Osiz().x;
    assert(XOsiz == siz->get_TOsiz().x);
    const uint32_t YOsiz = 0;
    assert(!siz->get_Osiz().y && !siz->get_TOsiz().y);
    const auto num_component = siz->get_Csiz();

    fixed_capacity_vector<std::array<pos2D, j2kprf::NL + 1>, j2kprf::Csiz_max> precinct_count{num_component};
    fixed_capacity_vector<uint32_t, j2kprf::Csiz_max> cmp_prc_count(num_component);

    switch (progression_order) {
        case j2kmk::RPCL:
            break;
        case j2kmk::PCRL:
            for (uint32_t y = 0; y < region.pos1.y; y += PPxy.y) {
                for (uint32_t x = 0; x < region.pos1.x; x += PPxy.x) {
                    for (const auto& cmp : tile_components) {
                        const auto c        = cmp.get_index();
                        const auto cXYRsiz  = siz->get_Rsiz(c);
                        const auto& cpsizes = cmp.acs_psizes();
                        for (const auto& rsl : cmp.acs_resolutions()) {
                            if (rsl.get_df_direction() == j2kmk::DFS_BOTH && (y % (PPxy.y * 2))) { continue; }

                            const auto r              = rsl.get_resolution_level();
                            const auto rprecinct_size = cpsizes[r];
                            const auto rxy            = rsl.get_resolution_xy();
                            const auto rnum_prc       = rsl.get_num_precinct();
                            bool xc1, xc2, xc3, yc1, yc2, yc3;

                            xc1 = x % (cXYRsiz.x * (1 << (rprecinct_size.x + j2kprf::NL - rxy.x))) == 0;
                            xc2 = x == XOsiz;
                            xc3 = !XOsiz * (1 << (j2kprf::NL - rxy.x) % (1 << (rprecinct_size.x + j2kprf::NL - rxy.x)));
                            if (!(xc1 || xc2 && xc3)) { continue; }
                            yc1 = y % (cXYRsiz.y * (1 << (rprecinct_size.y + j2kprf::NL - rxy.y))) == 0;
                            yc2 = y == YOsiz;
                            yc3 = !YOsiz * (1 << (j2kprf::NL - rxy.y) % (1 << (rprecinct_size.y + j2kprf::NL - rxy.y)));
                            if (!(yc1 || yc2 && yc3)) { continue; }

                            auto& current_pcount = precinct_count[c][r];

                            const auto PID = c + (cmp_prc_count[c]++) * num_component;
                            // std::cout << PID << std::endl;

                            const auto p = current_pcount.x + current_pcount.y * rnum_prc.x;

                            const auto xy     = pos2D{p % rnum_prc.x, p / rnum_prc.x};
                            const auto p_pos0 = pos2D::max(rsl.get_region().pos0, rprecinct_size.pow2() * xy);
                            const auto p_pos1 = pos2D::min(rsl.get_region().pos1, rprecinct_size.pow2() * (xy + 1));
                            table.emplace_back(
                                *this,
                                cmp, j2k_region<uint32_t>{p_pos0, p_pos1},
                                static_cast<uint8_t>(rsl.acs_subbands().size()), PID
                            );

                            current_pcount.x += 1;
                            if (current_pcount.x == rnum_prc.x) {
                                current_pcount.x = 0;
                                current_pcount.y += 1;
                            }
                        }
                    }
                }
            }
            break;
        case j2kmk::LRCP:
            [[fallthrough]];
        case j2kmk::RLCP:
            [[fallthrough]];
        case j2kmk::CPRL:
            [[fallthrough]];
        default:
            fprintf(stderr, "progression order %d is unsupported value\n", static_cast<int>(progression_order));
            exit(1);
    }
}
