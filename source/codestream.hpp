#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include "multi_memory.hpp"
#include "const_value.hpp"

#include "RTP_header.hpp"

template <typename T = uint32_t>
struct Postion2D {
    T x;
    T y;
    constexpr Postion2D() noexcept : x{0}, y{0} {}
    constexpr Postion2D(const T& in) noexcept : x{in}, y{in} {}
    constexpr Postion2D(const T& in_x, const T& in_y) noexcept : x{in_x}, y{in_y} {}
    constexpr T min() const noexcept { return x < y ? x : y; }
    constexpr T max() const noexcept { return x > y ? x : y; }
    constexpr T sum() const noexcept { return x + y; }
    constexpr T pro() const noexcept { return x * y; }
    constexpr Postion2D<T> pow2() const noexcept { return {static_cast<T>(1 << this->x), static_cast<T>(1 << this->y)}; }
    constexpr Postion2D<T> operator+(const T& in) const noexcept { return {this->x + in, this->y + in}; }
    constexpr Postion2D<T> operator-(const T& in) const noexcept { return {this->x - in, this->y - in}; }
    constexpr Postion2D<T> operator*(const T& in) const noexcept { return {this->x * in, this->y * in}; }
    constexpr Postion2D<T> operator/(const T& in) const noexcept { return {this->x / in, this->y / in}; }
    constexpr Postion2D<T> operator%(const T& in) const noexcept { return {this->x % in, this->y % in}; }
    constexpr Postion2D<T> operator+(const Postion2D<T>& in) const noexcept { return {this->x + in.x, this->y + in.y}; }
    constexpr Postion2D<T> operator-(const Postion2D<T>& in) const noexcept { return {this->x - in.x, this->y - in.y}; }
    constexpr Postion2D<T> operator*(const Postion2D<T>& in) const noexcept { return {this->x * in.x, this->y * in.y}; }
    constexpr Postion2D<T> operator/(const Postion2D<T>& in) const noexcept { return {this->x / in.x, this->y / in.y}; }
    constexpr Postion2D<T> operator%(const Postion2D<T>& in) const noexcept { return {this->x % in.x, this->y % in.y}; }
    static constexpr Postion2D<T> min(const Postion2D<T>& left, const Postion2D<T>& right) noexcept { return {(left.x < right.x) ? left.x : right.x, (left.y < right.y) ? left.y : right.y}; }
    static constexpr Postion2D<T> max(const Postion2D<T>& left, const Postion2D<T>& right) noexcept { return {(left.x > right.x) ? left.x : right.x, (left.y > right.y) ? left.y : right.y}; }
    constexpr Postion2D<T>& operator=(const Postion2D<T>& in) noexcept {
        this->x = in.x;
        this->y = in.y;
        return *this;
    }
    template <typename IN>
    constexpr Postion2D<T>& operator=(const Postion2D<IN>& in) noexcept {
        this->x = static_cast<T>(in.x);
        this->y = static_cast<T>(in.y);
        return *this;
    }
    template <typename IN>
    constexpr operator Postion2D<IN>() const noexcept { return {static_cast<IN>(this->x), static_cast<IN>(this->y)}; }
    typedef T type;
};
template <typename T>
constexpr Postion2D<T> operator+(const typename Postion2D<T>::type& left, const Postion2D<T>& right) noexcept { return {left + right.x, left + right.y}; }
template <typename T>
constexpr Postion2D<T> operator-(const typename Postion2D<T>::type& left, const Postion2D<T>& right) noexcept { return {left - right.x, left - right.y}; }
template <typename T>
constexpr Postion2D<T> operator*(const typename Postion2D<T>::type& left, const Postion2D<T>& right) noexcept { return {left * right.x, left * right.y}; }
template <typename T>
constexpr Postion2D<T> operator/(const typename Postion2D<T>::type& left, const Postion2D<T>& right) noexcept { return {left / right.x, left / right.y}; }
template <typename T>
constexpr Postion2D<T> operator%(const typename Postion2D<T>::type& left, const Postion2D<T>& right) noexcept { return {left % right.x, left % right.y}; }
using pos2D = Postion2D<>;

class J2kBuf {
public:
    //     inline static size_t count_true  = 0;
    //     inline static size_t count_false = 0;

    J2kBuf() : buf_ptr{nullptr}, bit_pos{128}, byte_pos{0}, buf_length{0}, recv{} {};
    J2kBuf(uint8_t* in) : buf_ptr{in}, bit_pos{128}, byte_pos{0}, buf_length{0}, recv{} {};
    J2kBuf(uint8_t* in, const size_t& length) : buf_ptr{in}, bit_pos{128}, byte_pos{0}, buf_length{length}, recv{} {};
    J2kBuf(uint8_t* in, const size_t& length, RTPReceiver* const& rptr)
        : buf_ptr{in}, bit_pos{128}, byte_pos{0}, buf_length{length}, recv{rptr} {};

    void step(const int64_t& = 1);
    void r_fill();
    void l_fill();
    void reset(uint8_t* const);

    uint8_t get_bit();
    uint32_t get_bit(const uint8_t&);

    uint8_t count_bit(const uint8_t&);

    uint8_t get_byte();
    uint32_t get_byte(const uint8_t&);

    void check_FF();

    uint8_t* get_ptr() const;

    uint8_t* make_packet_data(const size_t&, uint8_t* const);

private:
    inline void advance_byte_pos(const size_t&);
    inline void termination_check();
    void termination_check(const size_t&);
    void receive();
    alignas(8) uint8_t bit_pos;
    alignas(8) uint8_t bit_purge;

    RTPReceiver* recv;

    size_t byte_pos;
    size_t buf_length;
    uint8_t* buf_ptr;
};

// ceil(a / b);
template <typename A, typename B>
constexpr A ceil_int(const A& a, const B& b) { return (a + b - 1) / b; }

namespace j2kmk {
    enum {
        SOC = 0xFF4F,
        SOT = 0xFF90,
        SOD = 0xFF93,
        EOC = 0xFFD9,
        SIZ = 0xFF51,
        CAP = 0xFF50,
        COD = 0xFF52,
        COC = 0xFF53,
        RGN = 0xFF5E,
        QCD = 0xFF5C,
        QCC = 0xFF5D,
        POC = 0xFF5F,
        TLM = 0xFF55,
        PLM = 0xFF57,
        PLT = 0xFF58,
        PPM = 0xFF60,
        PPT = 0xFF61,
        DFS = 0xFF72,
        SOP = 0xFF91,
        EPH = 0xFF92,
        CRG = 0xFF63,
        COM = 0xFF64,

        LRCP = 0b0000,
        RLCP = 0b0001,
        RPCL = 0b0010,
        PCRL = 0b0011,
        CPRL = 0b0100,

        HT       = 0x040,
        HT_PHLD  = 0x100,
        HT_MIXED = 0x080,
        C_CAP15  = 0xC000,

        DFS_BOTH = 0b01,
        DFS_ROW  = 0b10,
        DFS_CAL  = 0b11

    };
}

class J2kMarkerBase {
public:
protected:
    uint16_t type;
    uint16_t length;
    uint8_t* buf_start;
};

class SOT {
public:
    SOT(J2kBuf&);

    uint16_t get_length() const;
    uint16_t get_Isot() const;
    uint32_t get_Psot() const;
    uint8_t get_TPsot() const;
    uint8_t get_TNsot() const;

private:
    uint16_t length;
    uint16_t Isot; // ラスタ順のタイル番号
    uint32_t Psot; // タイルパートのSOTを含む先頭から後尾までのバイト長 0のときはコードストリーム中の最後のタイルパートになりEOCまですべてのデータを含む
    uint8_t TPsot; // タイルパート番号
    uint8_t TNsot; // あるタイルに存在するタイルパート数
};

class SIZ {
public:
    SIZ(J2kBuf&);

    uint16_t get_Csiz() const;
    uint16_t get_decoder_profile() const;

    void get_tilesize(pos2D&) const;
    pos2D get_Siz() const;
    void get_Siz(pos2D&) const;
    void get_Osiz(pos2D&) const;
    pos2D get_Tsiz() const;
    void get_Tsiz(pos2D&) const;
    void get_TOsiz(pos2D&) const;
    uint8_t get_Ssiz(uint16_t) const;
    bool is_signed_component_sample(uint16_t) const;
    uint8_t get_bit_depth(uint16_t) const;
    void get_Rsiz(Postion2D<uint32_t>&, uint16_t) const;

    // Profile規定のチェック

    bool useDFS() const;

private:
    uint16_t Rsiz;                                      // Profile規定
    uint32_t Xsiz, Ysiz;                                // reference gridのサイズ
    uint32_t XOsiz, YOsiz;                              // reference gridの原点から左上端へのオフセット
    uint32_t XTsiz, YTsiz;                              // reference gridに対するタイルのサイズ
    uint32_t XTOsiz, YTOsiz;                            // reference gridの原点から先頭タイルまでのオフセット
    uint16_t Csiz;                                      // 画像中のコンポーネント数
    std::array<uint8_t, ConstValue::Csiz> Ssiz;         // ビット深度
    std::array<uint8_t, ConstValue::Csiz> XRsiz, YRsiz; // reference gridに対して何サンプル離れているか
    // siz-TOsizでreference gridと先頭タイルの左上が重なる
    // HTJ2Kでは,Rsizの14bitは1 (Rsiz | 0b0100 0000 0000 0000)
    // Rsiz が 1x00 xxxx xx1x xxxxのときDFS使用
};

class CAP {
public:
    CAP(J2kBuf&);

    uint16_t get_Ccap(uint8_t) const;

private:
    uint32_t Pcap;     // ?
    uint16_t Ccap[32]; // ?
};

class COD {
public:
    COD(J2kBuf&);

    bool get_precinct_type() const;
    bool get_is_SOP() const;
    bool get_is_EPH() const;
    uint8_t get_progression_order() const;
    uint16_t get_number_of_layers() const;
    uint8_t get_mct() const;
    uint8_t get_decomposition_level() const;
    void get_codeblock_size(pos2D&) const;
    uint8_t get_codeblock_style() const;
    uint8_t get_transformation() const;
    void get_precinct_size(pos2D&, const uint8_t) const;

private:
    uint8_t Scod;                                       // すべてのコンポーネントに対する符号化スタイル
    uint32_t SGcod;                                     // Scodで指定された符号化スタイル用のパラメータ コンポーネントに依存しない
    std::array<uint8_t, 5 + ConstValue::N_L + 1> SPcod; // Scodで指定された符号化スタイル用のパラメータ すべてのコンポーネントに関係
    /*
    // Scod
    bool precinct_type; // エントロピー符号で使用するpricinct
    bool is_SOP;        // SOPマーカーセグメント
    bool is_EPH;        // EPHマーカーセグメント

    // SGcod
    uint8_t progression_order;                // プログレッションの順番
    uint16_t number_of_layers;                // レイヤ数
    uint8_t multipe_component_transformation; // 複数コンポーネント間の変換あり

    // SPcod
    uint8_t number_of_decomposition_levels;    // 分割数N_L 解像度レベルは N_L + 1 max:32
    uint8_t codeblock_width, codeblock_height; // コードブロックのサイズ (x,y = 2 ^ (value + 2))
    uint8_t codeblock_style;                   // コードブロックのコーディングパスのスタイル
    uint8_t transformation;                    // ウェーブレット変換を使用
    std::vector<uint8_t> precinct_size;        // pricinct(境域)の水平(LSB)・垂直(MSB)サイズ
    std::vector<uint8_t> PPx;                  // pricinctの水平
    std::vector<uint8_t> PPy;                  // pricinctの垂直
    */
};

class COC {
public:
    COC(J2kBuf&, const uint16_t);

    uint16_t get_component_index() const;
    uint8_t get_coding_style() const;
    uint8_t get_decomposition_level() const;
    void get_codeblock_size(pos2D&) const;
    uint8_t get_codeblock_style() const;
    uint8_t get_transformation() const;
    void get_precinct_size(pos2D&, const uint8_t) const;

private:
    uint16_t Ccoc;                                      // COCマーカーが関係するコンポーネントのインデックス
    uint8_t Scoc;                                       // 本コンポーネントに対する符号化スタイル
    std::array<uint8_t, 5 + ConstValue::N_L + 1> SPcoc; // Scocで指定された符号化スタイル用のパラメータ
};

class RGN {
public:
    RGN(J2kBuf);

private:
};

class QCD {
public:
    QCD(J2kBuf&);

    uint8_t get_quantization_style() const;
    uint8_t get_number_of_guardbit() const;
    uint8_t get_quantization_step_size_exponent(const uint8_t) const;
    uint16_t get_quantization_step_size_mantissa(const uint8_t) const;

private:
    uint8_t Sqcd;                                        // すべてのコンポーネントに対する量子化スタイル
    std::array<uint16_t, ConstValue::num_subband> SPqcd; // i番目のサブバンドに対する量子化ステップ数
    // サブバンド数は10
};

class QCC {
public:
    QCC(J2kBuf&, const uint16_t);

    uint8_t get_component_index() const;
    uint8_t get_quantization_style() const;
    uint8_t get_number_of_guardbit() const;
    uint8_t get_quantization_step_size_exponent(const uint8_t) const;
    uint16_t get_quantization_step_size_mantissa(const uint8_t) const;

private:
    uint16_t Cqcc;                                       //
    uint8_t Sqcc;                                        //
    std::array<uint16_t, ConstValue::num_subband> SPqcc; //
};

class POC {
public:
    POC(J2kBuf&);

private:
    // 以下はプログレッションが変更になるたびに1つの値が割り当てられる
    std::vector<uint8_t> RSpoc;                  // プログレッション開始のための解像度レベルのインデックス番号
    std::vector<uint16_t> CSpoc;                 // プログレッション開始のためのコンポーネントのインデックス番号 Csizによって8bitか16bitになる
    std::vector<uint16_t> LYEpoc;                // プログレッションの末尾のレイヤインデックス番号
    std::vector<uint8_t> REpoc;                  // プログレッションの末尾の解像度レベルのインデックス番号
    std::vector<uint16_t> CEpoc;                 // プログレッションの末尾のコンポーネントのインデックス番号 Csizによって8bitか16bitになる
    std::vector<uint8_t> Ppoc;                   // プログレッション順序を示す
    uint32_t number_of_progression_order_change; // プログレッションの変更数
};

class TLM {
public:
    TLM(J2kBuf&);

private:
};

class PLM {
public:
    PLM(J2kBuf&);

private:
};

class PLT {
public:
    PLT(J2kBuf&);

private:
    uint8_t Zplt;              // 現在のヘッダに存在するほかのすべてのPLTマーカーセグメントに関係するマーカーセグメントのインデックス
    std::vector<uint8_t> Iplt; // i番目のパケット長 パケットヘッダがパケットと一緒のときパケットヘッダの長さを含む パケットヘッダがPPM or PPTのときは含まない
};

class PPM {
public:
    PPM(J2kBuf);

private:
};

class PPT {
public:
    PPT(J2kBuf&);

private:
};

class DFS {
public:
    DFS(J2kBuf&);

    uint16_t get_Sdfs() const;
    uint8_t get_Idfs() const;
    uint8_t get_Ddfs(const uint8_t) const;
    const uint8_t* get_Ddfs_data() const;

private:
    uint16_t Sdfs;                                 // このDFSマーカーセグメントのインデックス メインヘッダーのCOD,COCを介してコンポーネントに関連付けられる
                                                   // ->DFSが関連図けられるメインヘッダーのCOD,COCのインデックス,ここの値が1ならインデックスが1のコンポーネントにDFSを関連図ける
    uint8_t Idfs;                                  // Ddfsの要素数
    std::array<uint8_t, ConstValue::N_L + 1> Ddfs; // 各解像度におけるフィルタの方向 01:both 10:H(row) 11:V(col)
};

class SOP {
public:
    SOP(J2kBuf&);

private:
    uint16_t Nsop; // パケットシーケンス番号
};

class EPH {
public:
    EPH(J2kBuf&);

private:
};

class CRG {
public:
    CRG(J2kBuf&);

private:
};

class COM {
public:
    COM(J2kBuf&);
    // void print();

private:
    uint16_t Rcom;             // マーカーセグメントのレジストレーション値
    std::vector<uint8_t> Ccom; // コメント
    uint32_t index;            // このコメントが出現した番号
    // static uint32_t count;     // コメントの出現回数
    // static std::vector<COM*> comments; // コメントのポインタ
};

class UnknownMarker {
public:
    UnknownMarker(J2kBuf&);

private:
    std::vector<uint8_t> data; // 不明なマーカー内のデータを保存
};

struct MainHeader {
private:
    static constexpr size_t stack_size = [] {
        size_t out = 0;
        out += sizeof(SIZ);
        if (size_t mod = out % alignof(CAP); mod != 0) out += alignof(CAP) - mod;
        out += sizeof(CAP);
        if (size_t mod = out % alignof(COD); mod != 0) out += alignof(COD) - mod;
        out += sizeof(COD);
        if (size_t mod = out % alignof(COC); mod != 0) out += alignof(COC) - mod;
        out += sizeof(COC) * ConstValue::num_COC;
        if (size_t mod = out % alignof(QCD); mod != 0) out += alignof(QCD) - mod;
        out += sizeof(QCD);
        if (size_t mod = out % alignof(QCC); mod != 0) out += alignof(QCC) - mod;
        out += sizeof(QCC) * ConstValue::num_QCC;
        if (size_t mod = out % alignof(DFS); mod != 0) out += alignof(DFS) - mod;
        out += sizeof(DFS) * ConstValue::num_DFS;
        if (size_t mod = out % alignof(COM); mod != 0) out += alignof(COM) - mod;
        out += sizeof(COM) * ConstValue::num_COM;
        return out;
    }();
    MultiMem::static_memory<stack_size> mem;

public:
    MainHeader() {}
    MainHeader(J2kBuf&);
    void read(J2kBuf&);
    bool empty() { return !siz.operator bool(); }
    MultiMem::multi_ptr<SIZ> siz;
    MultiMem::multi_ptr<CAP> cap;
    MultiMem::multi_ptr<COD> cod;
    MultiMem::multi_ptr<COC> coc[ConstValue::num_COC];
    MultiMem::multi_ptr<QCD> qcd;
    MultiMem::multi_ptr<QCC> qcc[ConstValue::num_QCC];
    std::array<MultiMem::multi_ptr<DFS>, ConstValue::num_DFS> dfs;
    MultiMem::multi_ptr<COM> com[ConstValue::num_COM];
};

struct TilePartHeader {
private:
    // static constexpr size_t stack_size = [] {
    //     size_t out = 0;
    //     out += sizeof(SOT);
    //     return out;
    // }();
    // MultiMem::static_memory<stack_size> mem;

public:
    TilePartHeader(J2kBuf&, const uint16_t, uint32_t&);
    // MultiMem::multi_ptr<SOT> sot;
};