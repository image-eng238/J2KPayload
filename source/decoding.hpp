#pragma once
#include "codestream.hpp"
#include "multi_memory.hpp"
#include "const_value.hpp"

class TagTree;
class TagTreeNode;
class ReferenceGrid;
class CodeBlock;
class Subband;
class PrecinctSubband;
class Precinct;
class Resolution;
class Component;
class Tile_part;
class Tile;

// typedef MultiMem::static_memory<4194304> DecoMem;
// typedef MultiMem::static_memory<1551040> DecoMem; // 使う量ピッタリ
// typedef MultiMem::static_memory<1551016> DecoMem; // 使う量ピッタリ
// typedef MultiMem::static_memory<1551040 + 21640 + 30000> DecoMem; // Tile が 0x18'0000 でキリがいい
typedef MultiMem::static_memory<2000000> DecoMem; // Tile が 0x18'0000 でキリがいい
// typedef MultiMem::static_memory<0x1C'0000> DecoMem; // 0x18'0000 でキリがいい

class TagTree {
public:
    TagTree();
    TagTree(pos2D&);

    bool is_current_layer_inclusion(J2kBuf*, uint32_t, uint16_t);
    void read_buf(J2kBuf*, uint32_t);

    TagTreeNode* get_node(uint32_t);
    TagTreeNode* get_top_node();
    TagTreeNode* get_lowest_node(uint32_t);
    pos2D get_num_lowest_codeblock();
    uint32_t get_num_node();

private:
    pos2D num_codeblock;
    uint8_t maxlevel;
    uint32_t num_node;
    uint32_t lowest_level_start;
    std::unique_ptr<std::unique_ptr<TagTreeNode>[]> node;
};

class TagTreeNode {
public:
    TagTreeNode();
    TagTreeNode(TagTreeNode*, uint8_t, uint8_t);

    void add_child(TagTreeNode*);
    void add_value(uint32_t);
    void set_value(uint32_t);
    void confirm_value();

    uint8_t get_level();
    uint8_t get_value();
    uint32_t get_index();
    TagTreeNode* get_parent_ptr();
    TagTreeNode* get_child_ptr(uint8_t);

    bool set();

    operator bool() const;

private:
    void set_child_value(const uint32_t&);

    TagTreeNode* parent;
    TagTreeNode* child[4];
    uint32_t index;
    uint8_t num_child;
    uint8_t value;
    uint8_t level;
    bool is_set;
};

class alignas(8) ReferenceGrid {
public:
    pos2D get_pos0() const;
    pos2D get_pos1() const;

protected:
    /*8,4*/
    pos2D pos0; // 左上座標
    pos2D pos1; // 右下座標
};

class CodeBlock : public ReferenceGrid {
public:
    // friend void PrecinctSubband::read_packet_header(J2kBuf*, uint16_t, uint16_t, uint8_t);
    friend class PrecinctSubband;

    CodeBlock();
    void init(const pos2D&, const pos2D&, const pos2D&, const uint8_t, const uint8_t);
    void set_data(J2kBuf* const);
    void reuse();

private:
    /*16, 8*/
    // J2kBuf codeblock_data;
    uint8_t* codeblock_data;

    /*8, 4*/
    pos2D size;

    /*4, 4*/
    uint32_t length;

    /*2, 2*/
    uint16_t cmode;
    uint16_t number_of_layer;

    /*1, 1*/
    uint8_t number_of_coding_passes;
    uint8_t number_of_zbp;
    uint8_t fast_skep_passes;
    uint8_t lblock;
    uint8_t band;
    bool is_set;

    // std::vector<uint32_t> path_length;

    // this->length は RTP のパケットの最大サイズを超える可能性がある．
    // また，ハードウェアデコーダへデータを渡してからのバッファ寿命が不明．
    // そのため，一時的に class CodeBlock に static でバッファを持ち，
    // RTPで受信した符号化データを保持させる．

    static constexpr size_t NUM_BUFFER  = 5;
    static constexpr size_t BUFFER_SIZE = 0xFFFF;

    static inline uint8_t cbk_data_buffer[NUM_BUFFER][BUFFER_SIZE] = {};
    static inline size_t buffer_pos                                = 0;
};
class PrecinctSubband : public ReferenceGrid {
public:
    void init(const pos2D&, const pos2D&, const pos2D&, const uint8_t, const uint8_t, DecoMem* = nullptr);
    void read_packet_header(J2kBuf* const, const uint8_t = 0);
    pos2D get_number_of_codeblock() const;
    CodeBlock* get_codeblock_ptr(const uint32_t) const;

private:
    /*8, 8*/
    CodeBlock* codeblock;

    /*8, 4*/
    pos2D number_of_codeblock;

    /*1, 1*/
    uint8_t band_pos;
};
class Subband : public ReferenceGrid {
public:
    void init(const pos2D&, const pos2D&, const uint16_t, const uint8_t, const uint8_t, const uint8_t, const uint8_t, const uint8_t, const uint8_t);

    uint16_t get_mantissa_b() const;
    uint8_t get_subband_pos() const;
    uint8_t get_transformation() const;
    uint8_t get_R_b() const;
    uint8_t get_exponent_b() const;
    uint8_t get_M_b() const;

private:
    /*2, 2*/
    uint16_t mantissa_b;

    /*1, 1*/
    uint8_t subband_pos;
    uint8_t transformation;
    uint8_t R_b;
    uint8_t exponent_b;
    uint8_t M_b;
    uint8_t dfs;
};
class Precinct : public ReferenceGrid {
public:
    void init(const Subband* const, const uint8_t, const pos2D&, const pos2D&, const pos2D&, const uint32_t, const uint8_t, const uint8_t, DecoMem* = nullptr);

    PrecinctSubband* get_psubband_ptr(const uint8_t) const;
    uint8_t get_number_of_subband() const { return number_of_subband; }
    uint8_t get_resolution_level() const { return resolution_level; }

private:
    /*16, 8*/
    MultiMem::multi_ptr<PrecinctSubband> pband;

    /*4, 4*/
    uint32_t index;

    /*1, 1*/
    uint8_t number_of_subband;
    uint8_t resolution_level;
};
class Resolution : public ReferenceGrid {
public:
    void init(const pos2D&, const pos2D&, const pos2D&, const uint8_t, const uint8_t, const uint8_t);
    void set_precinct(const pos2D&, const pos2D&, const uint8_t, DecoMem* const = nullptr);
    void set_subband(const pos2D&, const pos2D&, const uint8_t, const uint8_t, const uint8_t, const uint8_t, const uint8_t, const std::array<uint8_t, ConstValue::num_subband>&, const std::array<uint16_t, ConstValue::num_subband>&, DecoMem* const = nullptr);

    bool empty() const;
    uint8_t get_number_of_subband() const;
    pos2D get_precinct_count() const;
    Precinct* get_precinct_ptr(const uint32_t) const;

private:
    /*16, 8*/
    MultiMem::multi_ptr<Precinct> precinct;
    MultiMem::multi_ptr<Subband> subband;

    /*8, 4*/
    pos2D precinct_count;

    /*1, 1*/
    uint8_t number_of_subband;
    uint8_t resolution_level;          // この解像度レベル, 0のときLL
    uint8_t downsampling_factor_style; // DFS不使用のときは0
    bool is_empty;
};
class Component : public ReferenceGrid {
public:
    Component();
    void init(const MainHeader&, const Tile&, const uint16_t);
    void set_resolution(DecoMem* = nullptr);
    void setCOC(const COC&, const DFS* const, const size_t&);
    void setQCC(const QCC&);
    void ceil_N_L(uint8_t& x, uint8_t& y) const;
    uint8_t get_N_L() const;
    pos2D get_precinct_size(const uint8_t) const;
    Resolution* get_resolution_ptr(const uint8_t) const;

private:
    /*16, 8*/
    MultiMem::multi_ptr<Resolution> resolution;

    /*8, 8*/
    const uint8_t* dfs_data;

    /*32, 4*/
    std::array<pos2D, ConstValue::N_L + 1> precinct_size; // pricinct(境域) の大きさ

    /*8, 4*/
    pos2D codeblock_size; // コードブロックの大きさ

    /*10, 2*/
    std::array<uint16_t, ConstValue::num_subband> q_mantissa;

    /*2, 2*/
    uint16_t index; // このコンポーネントのインデックス

    /*10 ,1*/
    std::array<uint8_t, ConstValue::num_subband> q_exponent;

    /*1, 1*/
    uint8_t bit_depth;       // ビット深度
    uint8_t N_L;             // 分割数
    uint8_t transformation;  // ウェーブレット変換を使用
    uint8_t codeblock_style; // コードブロックのコーディングパスのスタイル
    uint8_t quantization_style;
    uint8_t number_of_guardbit;
    bool is_DFS;
};

class Tile_part {
public:
    Tile_part(J2kBuf&);

    // TilePartHeader* get_header_ptr() const;
    uint8_t* get_data_ptr() const;

private:
    uint32_t length;
    uint16_t in_tile;
    uint8_t index;
    // std::unique_ptr<TilePartHeader> tile_part_header;
    uint8_t* data;
};

class Tile : public ReferenceGrid {
public:
    void init(const MainHeader&, J2kBuf&);
    void read(const MainHeader&, std::array<Precinct*, ConstValue::num_precinct * ConstValue::Csiz>&);
    void read_packet(const Precinct*, const uint16_t, const uint8_t, const uint8_t = 0);
    void read_packet(const Precinct* const current_precinct, J2kBuf& payload_buf);
    void setCOD(const COD&);
    void setQCD(const QCD&);

    void getCOC(uint8_t&, uint8_t&, uint8_t&, pos2D&, std::array<pos2D, ConstValue::N_L + 1>&) const;
    void getQCC(uint8_t&, uint8_t&, std::array<uint8_t, ConstValue::num_subband>&, std::array<uint16_t, ConstValue::num_subband>&) const;
    pos2D get_min_precinct_size() const;

private:
    /*1551048, 8*/
    DecoMem decoding_mem;

    //
    // static constexpr size_t mem_size = [] {
    //     size_t out = 0;
    //     out += sizeof(Tile_part);
    //     if (size_t mod = out / alignof(Component); mod != 0) out += alignof(Component) - mod;
    //     out += sizeof(Component) * 3;
    //     return out;
    // }();
    // MultiMem::static_memory<mem_size> mem;
    // これがあると家のPCで10~20usくらい速い(呼び出し時のスタックとかみ合わせがいいのかも)

    /*16, 8*/
    MultiMem::multi_ptr<Component> tile_component;
    MultiMem::multi_ptr<Tile_part> tile_part;
    J2kBuf tile_buf;

    /*48, 4*/
    std::array<pos2D, ConstValue::N_L + 1> precinct_size; // pricinct(境域) の大きさ

    /*8, 4*/
    pos2D codeblock_size; // コードブロックの大きさ

    /*4, 4*/
    uint32_t length;
    uint32_t number_of_packet; // このタイルにあるパケット数

    /*20, 2*/
    std::array<uint16_t, ConstValue::num_subband> q_mantissa;

    /*2, 2*/
    uint16_t number_of_component; // コンポーネント数
    uint16_t number_of_layer;     // レイヤ数
    uint16_t Ccap15;

    /*10, 1*/
    std::array<uint8_t, ConstValue::num_subband> q_exponent;

    /*1, 1*/
    uint8_t number_of_tilepart; // tile_part数
    uint8_t progression_order;  // プログレッションの順番
    uint8_t mct;                // multipe component transformation 複数コンポーネント間の変換あり
    uint8_t N_L;                // 分割数
    uint8_t codeblock_style;    // コードブロックのコーディングパスのスタイル
    uint8_t transformation;     // ウェーブレット変換を使用
    uint8_t quantization_style;
    uint8_t number_of_guardbit;
    bool precinct_type; // エントロピー符号で使用するpricinct
    bool is_SOP;        // SOPマーカーセグメント
    bool is_EPH;        // SOPマーカーセグメント
    bool is_DFS;        // DFSを使用しているかどうか
};