#include "codestream.hpp"
#include <iostream>
#include <algorithm>
#include <cstdio>

SOT::SOT(J2kBuf& in) {
    length = in.get_byte(2);
    Isot   = in.get_byte(2);
    Psot   = in.get_byte(4);
    TPsot  = in.get_byte();
    TNsot  = in.get_byte();
#ifdef GEN_PROPERTIES
    printf("> SOT\n");
    printf("  Isot: %d", get_Isot());
    printf("  Psot: %d", get_Psot());
    printf("  TPsot: %d", get_TPsot());
    printf("  TNsot: %d\n", get_TNsot());
#endif
}
uint16_t SOT::get_length() const { return length; }
uint16_t SOT::get_Isot() const { return Isot; }
uint32_t SOT::get_Psot() const { return Psot; }
uint8_t SOT::get_TPsot() const { return TPsot; }
uint8_t SOT::get_TNsot() const { return TNsot; }

SIZ::SIZ(J2kBuf& in) {
    in.step(2);
    Rsiz   = in.get_byte(2);
    Xsiz   = in.get_byte(4);
    Ysiz   = in.get_byte(4);
    XOsiz  = in.get_byte(4);
    YOsiz  = in.get_byte(4);
    XTsiz  = in.get_byte(4);
    YTsiz  = in.get_byte(4);
    XTOsiz = in.get_byte(4);
    YTOsiz = in.get_byte(4);
    Csiz   = in.get_byte(2);
    for (uint16_t i = 0; i < Csiz; ++i) {
        Ssiz[i]  = in.get_byte();
        XRsiz[i] = in.get_byte();
        YRsiz[i] = in.get_byte();
    }
#ifdef GEN_PROPERTIES
    printf("> SIZ\n");
    printf("  Rsiz: %d", Rsiz);
    printf("  Xsiz: %d", Xsiz);
    printf("  Ysiz: %d", Ysiz);
    printf("  XOsiz: %d", XOsiz);
    printf("  YOsiz: %d", YOsiz);
    printf("  XTsiz: %d", XTsiz);
    printf("  YTsiz: %d", YTsiz);
    printf("  XTOsiz: %d", XTOsiz);
    printf("  YTOsiz: %d", YTOsiz);
    printf("  Csiz: %d", Csiz);
    printf("  {Ssiz, XRsiz, YRsiz}");
    for (uint16_t i = 0; i < Csiz; ++i) {
        printf(" C[%d]: {%d, %d, %d}", i, Ssiz[i], XRsiz[i], YRsiz[i]);
    }
    printf("\n");

#endif
}
uint16_t SIZ::get_Csiz() const { return Csiz; };
void SIZ::get_tilesize(pos2D& in) const {
    in.x = ceil_int(Xsiz - XTOsiz, XTsiz);
    in.y = ceil_int(Ysiz - YTOsiz, YTsiz);
}
pos2D SIZ::get_Siz() const { return {Xsiz, Ysiz}; }
void SIZ::get_Siz(pos2D& in) const {
    in.x = Xsiz;
    in.y = Ysiz;
}
void SIZ::get_Osiz(pos2D& in) const {
    in.x = XOsiz;
    in.y = YOsiz;
}
pos2D SIZ::get_Tsiz() const { return {XTsiz, YTsiz}; }
void SIZ::get_Tsiz(pos2D& in) const {
    in.x = XTsiz;
    in.y = YTsiz;
}
void SIZ::get_TOsiz(pos2D& in) const {
    in.x = XTOsiz;
    in.y = YTOsiz;
}
uint8_t SIZ::get_Ssiz(uint16_t c) const { return Ssiz[c]; }
bool SIZ::is_signed_component_sample(uint16_t c) const { return Ssiz[c] >> 7; }
uint8_t SIZ::get_bit_depth(uint16_t c) const { return (Ssiz[c] & 0x7F) + 1; }
void SIZ::get_Rsiz(Postion2D<uint32_t>& in, uint16_t c) const {
    in.x = XRsiz[c];
    in.y = YRsiz[c];
}
bool SIZ::useDFS() const { return static_cast<bool>((Rsiz & 0x8000) && (~Rsiz & 0x3000) && (Rsiz & 0x0020)); }

CAP::CAP(J2kBuf& in) {
    in.step(2);
    Pcap = in.get_byte(4);
    for (uint32_t p = 1 << 31, i = 0; p; p >>= 1, ++i)
        if (Pcap & p) Ccap[i] = in.get_byte(2);
}
uint16_t CAP::get_Ccap(uint8_t i) const { return Ccap[i]; }

COD::COD(J2kBuf& in) {
    const uint16_t length = in.get_byte(2);
    Scod                  = in.get_byte();
    SGcod                 = in.get_byte(4);
    for (uint16_t i = 0; i < length - 7; ++i)
        SPcod[i] = in.get_byte();
}
bool COD::get_precinct_type() const { return Scod & 0b0000'0001; }
bool COD::get_is_SOP() const { return Scod & 0b0000'0010; }
bool COD::get_is_EPH() const { return Scod & 0b0000'0100; }
uint8_t COD::get_progression_order() const { return SGcod >> (010 * 3); }
uint16_t COD::get_number_of_layers() const { return (SGcod >> (010 * 1)) & 0xFFFF; }
uint8_t COD::get_mct() const { return SGcod & 0xF; }
uint8_t COD::get_decomposition_level() const { return SPcod[0]; }
void COD::get_codeblock_size(pos2D& in) const {
    in.x = 1 << (SPcod[1] + 2);
    in.y = 1 << (SPcod[2] + 2);
    return;
}
uint8_t COD::get_codeblock_style() const { return SPcod[3]; }
uint8_t COD::get_transformation() const { return SPcod[4]; }
void COD::get_precinct_size(pos2D& in, const uint8_t resolution) const {
    if (!get_precinct_type()) {
        in.x = 15;
        in.y = 15;
    } else {
        in.x = SPcod[resolution + 5] & 0x0F;
        in.y = (SPcod[resolution + 5] & 0xF0) >> 4;
    }
    return;
}
COC::COC(J2kBuf& in, const uint16_t Csiz) {
    const uint16_t length = in.get_byte(2);
    uint8_t n             = 1;
    if (Csiz < 257) {
        Ccoc = in.get_byte();
    } else {
        Ccoc = in.get_byte(2);
        n    = 2;
    }
    Scoc = in.get_byte();
    for (uint16_t i = 0; i < length - (3 + n); ++i)
        SPcoc[i] = in.get_byte();
}
uint16_t COC::get_component_index() const { return Ccoc; }
uint8_t COC::get_coding_style() const { return Scoc; }
uint8_t COC::get_decomposition_level() const { return SPcoc[0]; }
void COC::get_codeblock_size(pos2D& in) const {
    in.x = 1 << (SPcoc[1] + 2);
    in.y = 1 << (SPcoc[2] + 2);
    return;
}
uint8_t COC::get_codeblock_style() const { return SPcoc[3]; }
uint8_t COC::get_transformation() const { return SPcoc[4]; }
void COC::get_precinct_size(pos2D& in, const uint8_t resolution) const {
    if (!get_coding_style()) {
        in.x = 15;
        in.y = 15;
    } else {
        in.x = SPcoc[resolution + 5] & 0x0F;
        in.y = (SPcoc[resolution + 5] & 0xF0) >> 4;
    }
    return;
}

QCD::QCD(J2kBuf& in) {
    const uint16_t length = in.get_byte(2);
    Sqcd                  = in.get_byte();
    if (get_quantization_style() == 0) {
        for (uint16_t i = 0; i < length - 3; ++i)
            SPqcd[i] = in.get_byte(2);
    } else {
        for (uint16_t i = 0; i < (length - 3) / 2; ++i)
            SPqcd[i] = in.get_byte(2);
    }
}
uint8_t QCD::get_quantization_style() const { return Sqcd & 0b0001'1111; }
uint8_t QCD::get_number_of_guardbit() const { return Sqcd >> 7; }
uint8_t QCD::get_quantization_step_size_exponent(const uint8_t nb) const {
    const uint8_t qs = get_quantization_style();
    if (qs == 0) {
        return SPqcd[nb] >> 3;
    } else if (qs == 1) {
        return SPqcd[0] >> 11;
    } else {
        return SPqcd[nb] >> 11;
    }
}
uint16_t QCD::get_quantization_step_size_mantissa(const uint8_t nb) const {
    const uint8_t qs = get_quantization_style();
    if (qs == 0) {
        return SPqcd[nb] & 0b0000'0111;
    } else if (qs == 1) {
        return SPqcd[0] & 0b0000'0111'1111'1111;
    } else {
        return SPqcd[nb] & 0b0000'0111'1111'1111;
    }
}

QCC::QCC(J2kBuf& in, const uint16_t Csiz) {
    const uint16_t length = in.get_byte(2);
    uint8_t n             = 1;
    if (Csiz < 257) {
        Cqcc = in.get_byte();
    } else {
        Cqcc = in.get_byte(2);
        n    = 2;
    }
    Sqcc = in.get_byte();
    if (get_quantization_style() == 0) {
        for (uint16_t i = 0; i < length - (3 + 1); ++i)
            SPqcc[i] = in.get_byte();
    } else {
        for (uint16_t i = 0; i < (length - (3 + n)) / 2; ++i)
            SPqcc[i] = in.get_byte(2);
    }
}
uint8_t QCC::get_component_index() const { return Cqcc; }
uint8_t QCC::get_quantization_style() const { return Sqcc & 0b0001'1111; }
uint8_t QCC::get_number_of_guardbit() const { return Sqcc >> 5; }
uint8_t QCC::get_quantization_step_size_exponent(const uint8_t nb) const {
    const uint8_t qs = get_quantization_style();
    if (qs == 0) {
        return SPqcc[nb] >> 3;
    } else if (qs == 1) {
        return SPqcc[0] >> 11;
    } else {
        return SPqcc[nb] >> 11;
    }
}
uint16_t QCC::get_quantization_step_size_mantissa(const uint8_t nb) const {
    const uint8_t qs = get_quantization_style();
    if (qs == 0) {
        return SPqcc[nb] & 0b0000'0111;
    } else if (qs == 1) {
        return SPqcc[0] & 0b0000'0111'1111'1111;
    } else {
        return SPqcc[nb] & 0b0000'0111'1111'1111;
    }
}

DFS::DFS(J2kBuf& in) {
    in.step(2);
    Sdfs = in.get_byte(2);
    Idfs = in.get_byte();
    for (uint8_t i = 0; i < Idfs; ++i)
        Ddfs[i] = in.get_bit(2);
    Ddfs[Idfs] = Ddfs[Idfs - 1];
    std::reverse(Ddfs.begin(), Ddfs.end());
    in.r_fill();
}
uint16_t DFS::get_Sdfs() const { return Sdfs; }
uint8_t DFS::get_Idfs() const { return Idfs; }
uint8_t DFS::get_Ddfs(const uint8_t r) const { return Ddfs[r]; }
const uint8_t* DFS::get_Ddfs_data() const { return Ddfs.data(); }

COM::COM(J2kBuf& in) {
    const uint16_t length = in.get_byte(2);
    Rcom                  = in.get_byte(2);
    Ccom.resize(length - 4);
    for (uint16_t i = 0; i < length - 4; ++i)
        Ccom[i] = in.get_byte();
}

MainHeader::MainHeader(J2kBuf& in) {
    read(std::forward<J2kBuf&>(in));
}
void MainHeader::read(J2kBuf& in) {
    while (true) {
        switch (const uint16_t type = in.get_byte(2); type) {
            case j2kmk::SOC:
                break;
            case j2kmk::SOT:
                return;
            case j2kmk::SIZ:
                siz.set(mem, 1, in);
                break;
            case j2kmk::CAP:
                cap.set(mem, 1, in);
                break;
            case j2kmk::COD:
                cod.set(mem, 1, in);
                break;
            case j2kmk::COC: {
                size_t i = 0;
                while (coc[i] != nullptr) ++i;
                coc[i].set(mem, 1, in, siz->get_Csiz());
            } break;
            case j2kmk::QCD:
                qcd.set(mem, 1, in);
                break;
            case j2kmk::QCC: {
                size_t i = 0;
                while (qcc[i] != nullptr) ++i;
                qcc[i].set(mem, 1, in, siz->get_Csiz());
            } break;
            case j2kmk::DFS: {
                size_t i = 0;
                while (dfs[i] != nullptr) ++i;
                dfs[i].set(mem, 1, in);
            } break;
            case j2kmk::COM: {
                size_t i = 0;
                while (com[i] != nullptr) ++i;
                com[i].set(mem, 1, in);
            } break;
            default:
                std::cout << "unknown marker: " << std::hex << type << std::endl;
                if ((type >> 8) != 0xFF) {
                    return;
                }
                auto length = in.get_byte(2);
                in.step(length - 2);
        }
    }
}

TilePartHeader::TilePartHeader(J2kBuf& in, const uint16_t Csiz, uint32_t& header_size) {
    while (true) {
        switch (const uint16_t type = in.get_byte(2); type) {
            case j2kmk::SOD:
                header_size += 2;
                return;
                break;
            default:
                std::cout << "unknown marker: " << std::hex << type << std::endl;
                if ((type >> 8) != 0xFF) {
                    return;
                }
                header_size += 2 + (in.get_ptr()[0] << 4) | in.get_ptr()[1];
        }
    }
}