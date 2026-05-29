#include <cassert>
#include <iostream>
#include <vector>
#include <array>

#include "decoding.hpp"
#include "codestream.hpp"
#include "fast_table.hpp"

pos2D ReferenceGrid::get_pos0() const { return pos0; }
pos2D ReferenceGrid::get_pos1() const { return pos1; }

Component::Component() : is_DFS{false} {}
void Component::init(const MainHeader& mhd, const Tile& tile, const uint16_t c) {
    index     = c;
    bit_depth = mhd.siz->get_bit_depth(index);
    tile.getCOC(N_L, transformation, codeblock_style, codeblock_size, precinct_size);
    tile.getQCC(quantization_style, number_of_guardbit, q_exponent, q_mantissa);

    Postion2D<uint32_t> rsiz;
    mhd.siz->get_Rsiz(rsiz, index);
    pos0 = ceil_int(tile.get_pos0(), rsiz);
    pos1 = ceil_int(tile.get_pos1(), rsiz);

    for (auto& e : mhd.coc) {
        if (e->get_component_index() == index) {
            setCOC(*e, mhd.dfs.data()->get(), mhd.dfs.size());
        }
    }
    for (auto& e : mhd.qcc) {
        if (e->get_component_index() == index) {
            setQCC(*e);
        }
    }
}
void Component::set_resolution(DecoMem* decoding_mem) {
    resolution.set(*decoding_mem, N_L + 1);

    for (uint8_t r = 0; r <= N_L; ++r) {
        uint8_t rx = r, ry = r, num_subband = (r != 0) ? 3 : 1, dfs = 0;
        ceil_N_L(rx, ry);
        if (is_DFS && (dfs_data[r] & 0b10 && r != 0)) num_subband = 1;
        pos2D d       = pos2D(1 << N_L - rx, 1 << N_L - ry);
        pos2D re_pos0 = ceil_int(pos0, d);
        pos2D re_pos1 = ceil_int(pos1, d);
        pos2D pcount  = ceil_int(re_pos1, precinct_size[r].pow2()) - re_pos0 / precinct_size[r].pow2();

        resolution[r].init(pcount, re_pos0, re_pos1, r, num_subband, dfs);
        resolution[r].set_subband(pos0, pos1, bit_depth, N_L, transformation, quantization_style, number_of_guardbit, q_exponent, q_mantissa, decoding_mem);
        resolution[r].set_precinct(precinct_size[r], codeblock_size, codeblock_style, decoding_mem);
    }
}
void Component::setCOC(const COC& in, const DFS* const dfs, const size_t& size) {
    N_L = in.get_decomposition_level();
    in.get_codeblock_size(codeblock_size);
    codeblock_style = in.get_codeblock_style();
    transformation  = in.get_transformation();
    if (N_L & 0x80) {
        uint8_t dfs_idx = N_L & 0x7F;
        for (size_t i = 0; i < size; ++i) {
            if (dfs[i].get_Sdfs() == dfs_idx) {
                N_L      = dfs[i].get_Idfs();
                is_DFS   = true;
                dfs_data = dfs[i].get_Ddfs_data();
            }
        }
    }
    for (uint8_t i = 0; i <= N_L; ++i) {
        in.get_precinct_size(precinct_size[i], i);
    }
}
void Component::setQCC(const QCC& in) {
    quantization_style = in.get_quantization_style();
    number_of_guardbit = in.get_number_of_guardbit();

    uint8_t resiz = [&] {
        if (!is_DFS) return static_cast<uint8_t>(N_L * 3 + 1);
        uint8_t out = 1;
        for (uint8_t i = 0; i < N_L; ++i) {
            out += (dfs_data[i] == j2kmk::DFS_BOTH) ? 3 : 1;
        }
        return out;
    }();

    if (quantization_style != 1) {
        for (uint32_t nb = 0; nb < resiz; ++nb) {
            q_exponent[nb] = in.get_quantization_step_size_exponent(nb);
            if (quantization_style == 2) {
                q_mantissa[nb] = in.get_quantization_step_size_mantissa(nb);
            }
        }
        for (uint32_t f = resiz; f < q_exponent.size(); ++f) {
            q_exponent[f] = 0;
            if (quantization_style == 2) q_mantissa[f] = 0;
        }
    } else {
        q_exponent[0] = in.get_quantization_step_size_exponent(0);
        q_mantissa[0] = in.get_quantization_step_size_mantissa(0);
    }
}
void Component::ceil_N_L(uint8_t& x, uint8_t& y) const {
    assert(x == y || x <= N_L);
    if (is_DFS) {
        for (; x < N_L; ++x) {
            if (dfs_data[x + 1] != j2kmk::DFS_CAL) break;
        }
        for (; y < N_L; ++y) {
            if (dfs_data[y + 1] != j2kmk::DFS_ROW) break;
        }
    }
    return;
}
uint8_t Component::get_N_L() const { return N_L; }
pos2D Component::get_precinct_size(const uint8_t r) const { return precinct_size[r]; }
Resolution* Component::get_resolution_ptr(const uint8_t r) const { return &resolution[r]; };

Tile_part::Tile_part(J2kBuf& in) {
    uint32_t header_size = 2 + in.get_byte(2) + 2;
    // tile_part_header     = std::make_unique<TilePartHeader>(in, Csiz, header_size);
    in_tile              = in.get_byte(2);
    length               = in.get_byte(4);
    index                = in.get_byte();
    in.step(3);
    data = in.get_ptr();
    // in.step(length - header_size); // 複数のタイルパートの処理のために後続のタイルパートヘッダまでポインタを進める．このタイルパートに含まれるデータは Tile_part::data で処理
}
// TilePartHeader* Tile_part::get_header_ptr() const { return tile_part_header.get(); }
uint8_t* Tile_part::get_data_ptr() const { return data; };

void Tile::init(const MainHeader& mhd, J2kBuf& buf) {
    number_of_component = mhd.siz->get_Csiz();
    setCOD(*mhd.cod);
    setQCD(*mhd.qcd);
    Ccap15 = (mhd.cap) ? mhd.cap->get_Ccap(14) : 0;
    is_DFS = mhd.siz->useDFS();

    tile_part.set(decoding_mem, 1, buf);
    number_of_tilepart++;
    pos0 = {0, 0};
    pos1 = pos2D::min(mhd.siz->get_Tsiz(), mhd.siz->get_Siz());

    tile_component.set(decoding_mem, 3);
    for (uint16_t c = 0; c < number_of_component; ++c) {
        tile_component[c].init(mhd, *this, c);
    }
    for (uint16_t c = 0; c < number_of_component; ++c) {
        tile_component[c].set_resolution(&decoding_mem);
    }
}
void Tile::setCOD(const COD& in) {
    precinct_type     = in.get_precinct_type();
    is_SOP            = in.get_is_SOP();
    is_EPH            = in.get_is_EPH();
    progression_order = in.get_progression_order();
    number_of_layer   = in.get_number_of_layers();
    mct               = in.get_mct();
    N_L               = in.get_decomposition_level();
    assert(N_L <= 32);
    in.get_codeblock_size(codeblock_size);
    codeblock_style = in.get_codeblock_style();
    transformation  = in.get_transformation();
    for (uint8_t i = 0; i <= N_L; ++i) {
        in.get_precinct_size(precinct_size[i], i);
    }
}
void Tile::setQCD(const QCD& in) {
    quantization_style = in.get_quantization_style();
    number_of_guardbit = in.get_number_of_guardbit();
    if (quantization_style != 1) {
        for (uint8_t nb = 0; nb < q_exponent.size(); ++nb) {
            q_exponent[nb] = in.get_quantization_step_size_exponent(nb);
            if (quantization_style == 2) {
                q_mantissa[nb] = in.get_quantization_step_size_mantissa(nb);
            }
        }
    } else {
        q_exponent[0] = in.get_quantization_step_size_exponent(0);
        q_mantissa[0] = in.get_quantization_step_size_mantissa(0);
    }
}
void Tile::getCOC(uint8_t& nl, uint8_t& tf, uint8_t& stl, pos2D& siz, std::array<pos2D, ConstValue::N_L + 1>& prc) const {
    nl  = N_L;
    tf  = transformation;
    stl = codeblock_style;
    siz = codeblock_size;
    prc = precinct_size;
    return;
}
void Tile::getQCC(uint8_t& qs, uint8_t& ngb, std::array<uint8_t, ConstValue::num_subband>& qexp, std::array<uint16_t, ConstValue::num_subband>& qman) const {
    qs   = quantization_style;
    ngb  = number_of_guardbit;
    qexp = q_exponent;
    qman = q_mantissa;
    return;
}
pos2D Tile::get_min_precinct_size() const {
    pos2D out(16, 16);
    pos2D temp;
    for (uint16_t c = 0; c < number_of_component; ++c) {
        for (uint16_t r = 0; r <= tile_component[c].get_N_L(); ++r) {
            temp  = tile_component[c].get_precinct_size(r);
            out.x = (out.x > temp.x) ? temp.x : out.x;
            out.y = (out.y > temp.y) ? temp.y : out.y;
        }
    }
    return out;
}

void Tile::read(const MainHeader& mhd, std::array<fast_table, ConstValue::num_precinct * ConstValue::Csiz>& table) {
    // for (uint16_t c = 0; c < number_of_component; ++c) {
    //     tile_component[c].set_resolution(&decoding_mem);
    // }

    std::vector<uint32_t> x_min;
    std::vector<uint32_t> y_min;
    pos2D pp;
    Resolution* current_resolution_ptr = nullptr;

    std::vector<std::vector<uint32_t>> x_count(number_of_component, std::vector<uint32_t>(N_L + 1));
    std::vector<std::vector<uint32_t>> y_count(number_of_component, std::vector<uint32_t>(N_L + 1));

    uint32_t max_res_precinct_count = 0;
    uint8_t max_N_L_count           = 0;
    for (uint16_t c = 0; c < number_of_component; ++c) {
        max_N_L_count = std::max(tile_component[c].get_N_L(), max_N_L_count);
        for (uint32_t r = 0; r <= tile_component[c].get_N_L(); ++r) {
            number_of_packet += tile_component[c].get_resolution_ptr(r)->get_precinct_count().pro();
            max_res_precinct_count = std::max(tile_component[c].get_resolution_ptr(r)->get_precinct_count().pro(), max_res_precinct_count);
        }
    }
    number_of_tilepart *= number_of_layer;

    size_t table_index = 0;

    // Layer Resolution Component Precinct
    std::vector<std::vector<std::vector<std::vector<bool>>>> is_packet_read(number_of_layer);
    for (auto& e : is_packet_read) {
        e.resize(max_N_L_count + 1);
        for (auto& f : e) {
            f.resize(number_of_component);
            for (auto& g : f) {
                g.resize(max_res_precinct_count, false);
            }
        }
    }
    // tile_buf = tile_part[0].get_data_ptr();
    // ここまでは Main Payload Header だけで実行可能．ここのtile_bufに入るポインタは後続の Body Payload Header から持ってくる必要がある
    // また，HTJ2K_decoder_v2のコードではtile_bufに全ての画像データが続いていることを前提にしているため，要改良
    // 実際にはthis->read_packet()でパケットを処理するまで使用しないため， Main Payload Header が確定して時点でテーブル化自体は可能
    // テーブル化に precinct のポインタをパケット処理の順番で，配列として保存することで実現

    switch (progression_order) {
        case j2kmk::LRCP:
            break;
        case j2kmk::RLCP:
            break;
        case j2kmk::RPCL:
            pp = get_min_precinct_size();
            x_min.push_back(pos0.x);
            for (uint32_t x = 0; x < pos1.x; x += (1 << pp.x)) {
                if (x > pos0.x) {
                    x_min.push_back(x);
                }
            }
            y_min.push_back(pos0.y);
            for (uint32_t y = 0; y < pos1.y; y += (1 << pp.y)) {
                if (y > pos0.y) {
                    y_min.push_back(y);
                }
            }

            for (uint8_t r = 0; r <= N_L; ++r) {                             // Resolution
                for (auto& y : y_min) {                                      // Precinct.y
                    for (auto& x : x_min) {                                  // Precinct.x
                        for (uint16_t c = 0; c < number_of_component; ++c) { // Component

                            if (uint8_t current_N_L = tile_component[c].get_N_L(); r <= current_N_L) {
                                pos2D rsiz;
                                mhd.siz->get_Rsiz(rsiz, c);
                                const pos2D c_pp = tile_component[c].get_precinct_size(r);
                                pos2D c_tosiz;
                                mhd.siz->get_TOsiz(c_tosiz);
                                pos2D c_osiz;
                                mhd.siz->get_Osiz(c_osiz);
                                current_resolution_ptr = tile_component[c].get_resolution_ptr(r);

                                const bool xcheck =
                                    (x % (rsiz.x * (1 << (c_pp.x + current_N_L - r))) == 0) ||
                                    ((x == c_osiz.x) &&
                                     !c_tosiz.x * (1 << (current_N_L - r) % (1 << (c_pp.x + current_N_L - r))));
                                const bool ycheck =
                                    (y % (rsiz.y * (1 << (c_pp.y + current_N_L - r))) == 0) ||
                                    ((y == c_osiz.y) &&
                                     !c_tosiz.y * (1 << (current_N_L - r) % (1 << (c_pp.y + current_N_L - r))));
                                if (xcheck && ycheck) {
                                    const uint32_t p                     = x_count[c][r] + y_count[c][r] * current_resolution_ptr->get_precinct_count().x;
                                    Precinct* const current_precinct_ptr = current_resolution_ptr->get_precinct_ptr(p);
                                    for (uint16_t l = 0; l < number_of_layer; ++l) { // Layer
                                        if (!is_packet_read[l][r][c][p]) {
                                            // std::cout << "--- y: " << y << ", x: " << x << std::endl;
                                            is_packet_read[l][r][c][p] = true;
                                            read_packet(current_precinct_ptr, l, current_resolution_ptr->get_number_of_subband());
                                        }
                                    }
                                    x_count[c][r] += 1;
                                    if (x_count[c][r] == current_resolution_ptr->get_precinct_count().x) {
                                        x_count[c][r] = 0;
                                        y_count[c][r] += 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            break;
        case j2kmk::PCRL:
            pp = get_min_precinct_size();
            pp = pos2D(9, 4);
            x_min.push_back(pos0.x);
            for (uint32_t x = 0; x < pos1.x; x += (1 << pp.x)) {
                if (x > pos0.x) {
                    x_min.push_back(x);
                }
            }
            y_min.push_back(pos0.y);
            for (uint32_t y = 0; y < pos1.y; y += (1 << pp.y)) {
                if (y > pos0.y) {
                    y_min.push_back(y);
                }
            }

            for (auto& y : y_min) {                                      // Precinct.y
                for (auto& x : x_min) {                                  // Precinct.x
                    for (uint16_t c = 0; c < number_of_component; ++c) { // Component
                        const uint8_t current_N_L = tile_component[c].get_N_L();
                        const uint8_t local_N_L   = std::min(static_cast<uint8_t>(current_N_L + 1), N_L);
                        uint32_t sp               = 0;

                        for (uint8_t r = 0; r <= local_N_L; ++r) { // Resolution

                            const pos2D c_pp       = tile_component[c].get_precinct_size(r);
                            current_resolution_ptr = tile_component[c].get_resolution_ptr(r);

                            if (!current_resolution_ptr->empty()) {
                                pos2D rsiz;
                                mhd.siz->get_Rsiz(rsiz, c);
                                pos2D c_tosiz;
                                mhd.siz->get_TOsiz(c_tosiz);
                                pos2D c_osiz;
                                mhd.siz->get_Osiz(c_osiz);

                                uint8_t rx = r, ry = r;
                                tile_component[c].ceil_N_L(rx, ry);

                                const bool xcheck =
                                    (x % (rsiz.x * (1 << (c_pp.x + current_N_L - rx))) == 0) ||
                                    ((x == c_osiz.x) &&
                                     !c_tosiz.x * (1 << (current_N_L - rx) % (1 << (c_pp.x + current_N_L - rx))));
                                if (!xcheck)
                                    continue;
                                const bool ycheck =
                                    (y % (rsiz.y * (1 << (c_pp.y + current_N_L - ry))) == 0) ||
                                    ((y == c_osiz.y) &&
                                     !c_tosiz.y * (1 << (current_N_L - ry) % (1 << (c_pp.y + current_N_L - ry))));
                                if (!ycheck)
                                    continue;
                                if (true) {
                                    const uint32_t p                     = x_count[c][r] + y_count[c][r] * current_resolution_ptr->get_precinct_count().x;
                                    Precinct* const current_precinct_ptr = current_resolution_ptr->get_precinct_ptr(p);
                                    ++sp;
                                    for (uint16_t l = 0; l < number_of_layer; ++l) { // Layer 今回は Layer は 1 固定
                                        if (!is_packet_read[l][r][c][p]) {
                                            is_packet_read[l][r][c][p] = true;
                                            // std::cout << "--- y: " << y << ", x: " << x << std::endl;
                                            uint32_t PID               = c + sp * number_of_component;
                                            table[table_index++].set(current_precinct_ptr, PID);
                                            // assert(table_index <= ConstValue::num_precinct * ConstValue::Csiz);
                                        }
                                    }
                                    x_count[c][r] += 1;
                                    if (x_count[c][r] == current_resolution_ptr->get_precinct_count().x) {
                                        x_count[c][r] = 0;
                                        y_count[c][r] += 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;
        case j2kmk::CPRL:
            break;
        default:
            std::cout << "unkown progression_order: " << progression_order << std::endl;
            return;
    }
    return;
}
void Tile::read_packet(const Precinct* current_precinct, const uint16_t layer, const uint8_t num_subband, const uint8_t debug_resolution) {
    if (!tile_buf.get_bit()) { // empty packet 今回は存在しない可能性あり
        std::cout << "empty packet" << std::endl;
        return;
    }

    PrecinctSubband* current_ps;
    for (uint8_t i = 0; i < num_subband; ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
        current_ps->read_packet_header(&tile_buf, debug_resolution);
    }
    tile_buf.check_FF();

    for (uint8_t i = 0; i < num_subband; ++i) {
        current_ps             = current_precinct->get_psubband_ptr(i);
        uint32_t num_codeblock = current_ps->get_number_of_codeblock().pro(); // 必ず1
        for (uint32_t cindex = 0; cindex < num_codeblock; ++cindex) {
            current_ps->get_codeblock_ptr(cindex)->set_data(&tile_buf);
        }
    }
}

void Tile::read_packet(const Precinct* const current_precinct, J2kBuf& payload_buf) {
    static size_t call_count = 0;
    call_count++;
    if (!payload_buf.get_bit()) { // empty packet
        std::cout << "empty packet, call_count: " << call_count << std::endl;
        exit(1);
        return;
    }

    PrecinctSubband* current_ps;
    for (uint8_t i = 0; i < current_precinct->get_number_of_subband(); ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
#ifdef GENERATE_LOG
        current_ps->read_packet_header(&payload_buf, current_precinct->get_resolution_level());
#else
        current_ps->read_packet_header(&payload_buf);
#endif
    }
    payload_buf.check_FF();
    payload_buf.r_fill();

    for (uint8_t i = 0; i < current_precinct->get_number_of_subband(); ++i) {
        current_ps = current_precinct->get_psubband_ptr(i);
        current_ps->get_codeblock_ptr(0)->set_data(&payload_buf);
    }
}