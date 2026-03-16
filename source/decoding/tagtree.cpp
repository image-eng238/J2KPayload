#include "codestream.hpp"
#include "decoding.hpp"

#include <algorithm>
#include <cassert>

TagTree::TagTree() : maxlevel(0) {}
TagTree::TagTree(pos2D& in) : num_codeblock(in), maxlevel(0), num_node(0), lowest_level_start(0) {
    // for (uint32_t i = num_codeblock.max(); i != 1; i = ceil_int(i, 2), ++maxlevel);
    // node = std::make_unique<TagTreeNode>(num_codeblock, maxlevel);

    num_node = 1;
    node     = std::make_unique<std::unique_ptr<TagTreeNode>[]>(num_node);
    node[0]  = std::make_unique<TagTreeNode>(nullptr, 0, 0); // 最上位の親の作成

    // std::vector<pos2D> num_cbks;
    // num_cbks.push_back(num_codeblock);
    // // pos2D current_num_cdb = num_codeblock;
    // while (true) {
    //     num_node += num_cbks.back().pro();
    //     if (num_cbks.back().pro() <= 1) {
    //         break;
    //     } else {
    //         // current_num_cdb = ceil_int(current_num_cdb, 2);
    //         num_cbks.push_back(ceil_int(num_cbks.back(), 2));
    //         ++maxlevel;
    //     }
    // }
    // std::reverse(num_cbks.begin(), num_cbks.end()); // 子->親からに親->子に反転
    // node    = std::make_unique<std::unique_ptr<TagTreeNode>[]>(num_node);
    // node[0] = std::make_unique<TagTreeNode>(nullptr, 0, 0); // 最上位の親の作成
    // for (uint32_t depth = 1, d = 1; depth < num_node; depth += num_cbks[d++].pro()) {
    //     for (uint32_t y = 0; y < num_cbks[d].y; ++y) {
    //         for (uint32_t x = 0; x < num_cbks[d].x; ++x) {
    //             uint32_t pos        = depth + x + (y * num_cbks[d].x);
    //             uint32_t parent_pos = lowest_level_start + (x / 2) + (y / 2 * num_cbks[d - 1].x);
    //             node[pos]           = std::make_unique<TagTreeNode>(node[parent_pos].get(), d, pos);
    //         }
    //     }
    //     lowest_level_start = depth;
    // }
}

bool TagTree::is_current_layer_inclusion(J2kBuf* inbuf, uint32_t c_index, uint16_t c_layer) {
    std::vector<uint32_t> child_parent;
    TagTreeNode* current = get_lowest_node(c_index);
    do {
        child_parent.push_back(current->get_index());
        current = current->get_parent_ptr();
    } while (current != nullptr && !*current);
    std::reverse(child_parent.begin(), child_parent.end());

    bool output = false;
    for (auto& e : child_parent) {
        if (node[e]->set() == false && node[e]->get_value() <= c_layer) {
            uint8_t bit = inbuf->get_bit();
            if (bit) {
                node[e]->confirm_value();
                output = true;
            } else {
                node[e]->add_value(1);
                output = false;
            }
        }
    }
    return output;
}

void TagTree::read_buf(J2kBuf* inbuf, uint32_t c_index) {
    assert(get_lowest_node(c_index)->set() == false);
    std::vector<uint32_t> child_parent;
    TagTreeNode* current = get_lowest_node(c_index);
    do {
        child_parent.push_back(current->get_index());
        current = current->get_parent_ptr();
    } while (current != nullptr && !*current);
    std::reverse(child_parent.begin(), child_parent.end());

    for (auto& e : child_parent) {
        uint8_t bits = 0;
        while (!inbuf->get_bit()) ++bits;
        node[e]->add_value(bits);
        node[e]->confirm_value();
    }
    return;
}

TagTreeNode* TagTree::get_node(uint32_t i) { return node[i].get(); }
TagTreeNode* TagTree::get_top_node() { return node[0].get(); }
TagTreeNode* TagTree::get_lowest_node(uint32_t i) { return node[lowest_level_start + i].get(); }
pos2D TagTree::get_num_lowest_codeblock() { return num_codeblock; }
uint32_t TagTree::get_num_node() { return num_node; }

/* TagTreeNode */
/*__tagtreenode __TagTreeNode __ttn*/
TagTreeNode::TagTreeNode() : parent(nullptr) {}
TagTreeNode::TagTreeNode(TagTreeNode* p, uint8_t l, uint8_t i)
    : parent(p), level(l), index(i),
      child{nullptr}, num_child(0), value(0), is_set(false) {
    if (parent != nullptr) {
        parent->add_child(this);
    }
}

void TagTreeNode::add_child(TagTreeNode* p) { child[num_child++] = p; }

void TagTreeNode::add_value(uint32_t n) {
    assert(is_set == false);
    value += n;
    for (uint8_t i = 0; i < num_child; ++i) {
        child[i]->set_child_value(value);
    }
}

void TagTreeNode::set_value(uint32_t n) {
    is_set = true;
    value  = n;
}
void TagTreeNode::confirm_value() { is_set = true; }

uint8_t TagTreeNode::get_level() { return level; }
uint8_t TagTreeNode::get_value() { return value; }
uint32_t TagTreeNode::get_index() { return index; }
TagTreeNode* TagTreeNode::get_parent_ptr() { return parent; }
TagTreeNode* TagTreeNode::get_child_ptr(uint8_t i) { return child[i]; }

bool TagTreeNode::set() { return is_set; }

TagTreeNode::operator bool() const { return is_set; }

void TagTreeNode::set_child_value(const uint32_t& n) {
    assert(is_set == false);
    value = n;
    for (uint8_t i = 0; i < num_child; ++i) {
        child[i]->set_child_value(n);
    }
}