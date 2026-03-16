#include "decoding.hpp"
#include <cmath>
#include <iostream>

CodeBlock::CodeBlock() : size{}, length(0), cmode(0), number_of_layer(0), number_of_coding_passes(0), number_of_zbp(0), fast_skep_passes(0), lblock(0), band(0), is_set(false) {}
void CodeBlock::init(const pos2D& cb_pos0, const pos2D& cb_pos1, const pos2D& siz, const uint8_t cstyle, const uint8_t b) {
    pos0            = cb_pos0;
    pos1            = cb_pos1;
    size            = siz;
    cmode           = cstyle;
    number_of_layer = 1;
    band            = b;

    // path_length.resize(layer);
}
void CodeBlock::set_data(J2kBuf* const buf) {
    is_set         = true;
    codeblock_data = *buf;
    buf->r_fill();
    buf->step(length);
}

void PrecinctSubband::init(const pos2D& ps_pos0, const pos2D& ps_pos1, const pos2D& csiz, const uint8_t csl, const uint8_t spos, DecoMem* decoding_mem) {
    pos0                 = ps_pos0;
    pos1                 = ps_pos1;
    band_pos             = spos;
    number_of_codeblock  = {1, 1};
    codeblock            = decoding_mem->get<CodeBlock>();
    // inclusion.set(*decoding_mem, 1, number_of_codeblock);
    // zero_bit_plane.set(*decoding_mem, 1, number_of_codeblock);
    const pos2D cbl_tl   = pos2D::max(pos0, csiz * (pos1 / csiz));
    const pos2D cbl_br   = pos2D::min(pos1, csiz * (1 + pos0 / csiz));
    const pos2D cbl_size = cbl_br - cbl_tl;
    codeblock->init(cbl_tl, cbl_br, cbl_size, csl, band_pos);
};
void PrecinctSubband::read_packet_header(J2kBuf* const buf, const uint8_t debug_resolution) {
    // for (uint32_t i = 0; i < number_of_codeblock.pro(); ++i) {
    CodeBlock* const current_block = codeblock;
    // bool is_included               = false;

    // codeblock inclusion のタグツリーを読んでこのコードブロックに圧縮データがあるか判定
    // is_included = inclusion->is_current_layer_inclusion(buf, i, layer);
    // is_included = [&] {
    //     return static_cast<bool>(buf->get_bit());
    // }();

    if (buf->get_bit()) {
        // zero_bit_plane->read_buf(buf, i);
        // current_block->number_of_zbp = zero_bit_plane->get_lowest_node(i)->get_value();
        current_block->number_of_zbp = [&] {
            uint8_t bits = 0;
            while (!buf->get_bit()) ++bits;
            return bits;
        }();
        current_block->lblock = 3;
        // 符号化パス数を読む //直す
        // uint32_t new_pass            = 1;
        // new_pass += buf->get_bit();
        // if (new_pass >= 2) {
        //     new_pass += buf->get_bit();
        //     if (new_pass >= 3) {
        //         new_pass += buf->get_bit(2);
        //         if (new_pass >= 6) {
        //             new_pass += buf->get_bit(5);
        //             if (new_pass >= 37) {
        //                 new_pass += buf->get_bit(7);
        //             }
        //         }
        //     }
        // }
        buf->get_bit();
        while (buf->get_bit()) current_block->lblock++;

        current_block->length = buf->get_bit(current_block->lblock);
        assert(current_block->length > 1);

#if defined(GENERATE_LOG)
        // std::cout << std::dec << call_count++ << " segment_byte: " << segment_byte << std::endl;
        // std::cout << std::dec << ++call_count << ": " << static_cast<uint32_t>(debug_resolution) << "," << static_cast<uint32_t>(band_pos) << "," << segment_byte << std::endl;
        std::cout << static_cast<uint32_t>(debug_resolution) << "," << static_cast<uint32_t>(band_pos) << "," << current_block->length << std::endl;
#endif
    } else {
#if defined(GENERATE_LOG)
        // std::cout << std::dec << ++call_count << ": " << static_cast<uint32_t>(debug_resolution) << "," << static_cast<uint32_t>(band_pos) << ",0" << std::endl;
        std::cout << static_cast<uint32_t>(debug_resolution) << "," << static_cast<uint32_t>(band_pos) << ",0" << std::endl;
#endif
        // }
    }
}
pos2D PrecinctSubband::get_number_of_codeblock() const { return number_of_codeblock; }
CodeBlock* PrecinctSubband::get_codeblock_ptr(const uint32_t i) const { return &(codeblock[i]); }

void Subband::init(const pos2D& sb_pos0, const pos2D& sb_pos1, const uint16_t mantissa, const uint8_t b, const uint8_t tf, const uint8_t R_b, const uint8_t exp, const uint8_t M_b, const uint8_t in_dfs) {
    pos0           = sb_pos0;
    pos1           = sb_pos1;
    mantissa_b     = mantissa;
    subband_pos    = b;
    transformation = tf;
    this->R_b      = R_b;
    exponent_b     = exp;
    this->M_b      = M_b;
    this->dfs      = in_dfs;
}
uint16_t Subband::get_mantissa_b() const { return mantissa_b; }
uint8_t Subband::get_subband_pos() const { return subband_pos; }
uint8_t Subband::get_transformation() const { return transformation; }
uint8_t Subband::get_R_b() const { return R_b; }
uint8_t Subband::get_exponent_b() const { return exponent_b; }
uint8_t Subband::get_M_b() const { return M_b; }

void Precinct::init(const Subband* const subband, const uint8_t num_subband, const pos2D& p_pos0, const pos2D& p_pos1, const pos2D& csiz, const uint32_t idx, const uint8_t r, const uint8_t csl, DecoMem* decoding_mem) {
    pos0              = p_pos0;
    pos1              = p_pos1;
    index             = idx;
    resolution_level  = r;
    number_of_subband = num_subband;
    pband.set(*decoding_mem, number_of_subband);
    const pos2D o_b[4] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (uint8_t i = 0; i < number_of_subband; ++i) {
        const uint8_t spos = subband[i].get_subband_pos();
        const uint8_t sr   = (spos == 0) ? 1 << 0 : 1 << 1;

        const pos2D nb_tl = ceil_int(p_pos0 - o_b[spos], sr);
        const pos2D nb_br = ceil_int(p_pos1 - o_b[spos], sr);

        pband[i].init(nb_tl, nb_br, csiz, csl, spos, decoding_mem);
        // printf("tl: %d,%d, br: %d,%d\n", tl.x, tl.y, br.x, br.y);
    }

#ifdef IS_COUNT_STRUCT
    num.precinct++;
#endif

    return;
}
PrecinctSubband* Precinct::get_psubband_ptr(const uint8_t b) const { return &pband[b]; }

void Resolution::init(const pos2D& pcount, const pos2D& re_pos0, const pos2D& re_pos1, const uint8_t r, const uint8_t num_subband, const uint8_t dfs) {
    precinct_count            = pcount;
    pos0                      = re_pos0;
    pos1                      = re_pos1;
    is_empty                  = !(precinct_count.sum());
    resolution_level          = r;
    number_of_subband         = num_subband;
    downsampling_factor_style = dfs;
}
void Resolution::set_precinct(const pos2D& psiz, const pos2D& csiz, const uint8_t cstyle, DecoMem* const decoding_mem) {
    if (!is_empty) {
        precinct.set(*decoding_mem, precinct_count.pro());
        const pos2D offset(pos0 / psiz.pow2());
        for (uint32_t i = 0; i < precinct_count.pro(); ++i) {
            const pos2D xy(i % precinct_count.x, i / precinct_count.x);
            const pos2D p_pos0 = pos2D::max(pos0, psiz.pow2() * (xy + offset));
            const pos2D p_pos1 = pos2D::min(pos1, psiz.pow2() * (xy + 1 + offset));
            precinct[i].init(subband.get(), number_of_subband, p_pos0, p_pos1, csiz, i, resolution_level, cstyle, decoding_mem);
        }
    }
}
void Resolution::set_subband(const pos2D& c_pos0, const pos2D& c_pos1, const uint8_t bit_depth, const uint8_t N_L, const uint8_t transformation, const uint8_t qsl, const uint8_t ngb, const std::array<uint8_t, ConstValue::num_subband>& exp, const std::array<uint16_t, ConstValue::num_subband>& man, DecoMem* const decoding_mem) {
    subband.set(*decoding_mem, number_of_subband);

    const pos2D o_b[4]    = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    const uint8_t gain[4] = {0, 1, 1, 2};
    const uint8_t nb      = (resolution_level != 0) ? N_L + 1 - resolution_level : N_L + 1 - resolution_level - 1;
    uint8_t b_pos         = (resolution_level != 0) ? 1 : 0;
    if (downsampling_factor_style & 0b10) {
        b_pos += downsampling_factor_style & 0b01;
    }

    for (uint8_t sb = 0; sb < number_of_subband; ++sb) {
        const pos2D sb_pos0 = ceil_int(c_pos0 - (o_b[b_pos] * (1 << (nb - 1))), 1 << nb);
        const pos2D sb_pos1 = ceil_int(c_pos1 - (o_b[b_pos] * (1 << (nb - 1))), 1 << nb);
        uint8_t exponent_b, R_b, M_b;
        uint16_t mantissa_b;

        if (transformation == 1) { // lossless
            exponent_b = exp[3 * (N_L - nb) + b_pos];
            M_b        = exponent_b + ngb - 1;
            R_b        = 0;
        } else {
            if (qsl == 1) {
                exponent_b = exp[0] - N_L + nb;
                mantissa_b = man[0];
            } else {
                // if(qsl == 2)
                exponent_b = exp[3 * (N_L - nb) + b_pos];
                mantissa_b = man[3 * (N_L - nb) + b_pos];
            }
            M_b = exponent_b + ngb - 1;
            R_b = bit_depth + gain[b_pos];
        }

        subband[sb].init(sb_pos0, sb_pos1, mantissa_b, b_pos, transformation, R_b, exponent_b, M_b, downsampling_factor_style);
        // b += b_step;
        b_pos++;
    }
}
bool Resolution::empty() const { return is_empty; }
uint8_t Resolution::get_number_of_subband() const { return number_of_subband; }
pos2D Resolution::get_precinct_count() const { return precinct_count; }
Precinct* Resolution::get_precinct_ptr(const uint32_t p) const { return &precinct[p]; }