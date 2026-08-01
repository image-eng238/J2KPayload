#include "RTP_header.hpp"

int32_t RTPReceiver::first_check() {
    using namespace RTPHeader_trait;
    using namespace J2KPayloadHeader_trait;
    const auto hl = RTPHeader_trait::get_header_length();
    pos           = 0;

    while (true) {
        auto& data = packets[0].data;
        auto& len  = packets[0].len;

        len = buffer->pop(data);
        if (unlikely(len == 1 && RTPHeader_trait::get_V(data) != 0b10)) return this->FINISH;

        const auto sequence = get_extended_sequence_number(data);
        pre_sequence_number = sequence;

        if (likely(get_MH(data + hl))) { // メインヘッダ出現
            num_packets         = 1;
            cache               = {};
            packets[1].len      = buffer->pop(packets[1].data);
            pre_sequence_number = get_extended_sequence_number(packets[1].data);
            ++num_packets;
            return this->MAIN_HEADER;
        }
    }
}

int32_t RTPReceiver::check() {
    using namespace RTPHeader_trait;
    using namespace J2KPayloadHeader_trait;
    const auto hl = RTPHeader_trait::get_header_length();

    cache = packets[num_packets - 1];
    if (unlikely(is_EOC)) {
        is_EOC = false;
        return this->SUCCESS;
    }

    pos          = 0;
    num_packets  = 0;
    uint8_t rpos = 0;

    while (true) {
        auto& data = packets[rpos].data;
        auto& len  = packets[rpos].len;
        ++rpos;
        len = buffer->pop(data);
        if (unlikely(len == 1 && RTPHeader_trait::get_V(data) != 0b10)) return this->FINISH;
        ++num_packets;

        const auto sequence     = get_extended_sequence_number(data);
        const auto pre_sequence = pre_sequence_number;
        pre_sequence_number     = sequence;

        if (likely((sequence == pre_sequence + 1) || (pre_sequence == 0 && sequence == 1) || (pre_sequence == ex_sequence_max || sequence == 0))) {
            if (unlikely(get_MH(data + hl))) { // メインヘッダ出現
                assert(num_packets == 1);
                cache = {};
                return this->MAIN_HEADER;
            }

            if (unlikely(get_M(data))) { is_EOC = true; } // コードストリーム終端

            if (get_body_ORDB(data + hl)) { // 再同期ポイントが出現した場合 J2K パケットの解析が可能に
                PID = get_body_PID(data + hl);
                return this->SUCCESS;
            }
        } else {
            printf("pre: %d, seq: %d, diff: %d\n", pre_sequence, sequence, sequence - pre_sequence);
            // パケットロス発生 次の再同期ポイントまでパケットを破棄
            if (unlikely(sequence == 0)) {
                num_lost_packet = ex_sequence_max - pre_sequence;
            } else {
                num_lost_packet = sequence - (pre_sequence + 1);
                if (num_lost_packet >= buffer->get_buffer_length()) { throw buffer_leak{"RTP seq", buffer_leak::ANALYSISING}; }
            }
            while (true) {
                len = buffer->pop(data);
                if (unlikely(len == 1 && RTPHeader_trait::get_V(data) != 0b10)) return this->FINISH;
                if (!get_MH(data + hl) && get_body_ORDB(data + hl)) {
                    pre_sequence_number = get_extended_sequence_number(data);
                    PID                 = get_body_PID(data + hl);
                    return this->FAILURE;
                }
            }
        }
    }
}

void RTPReceiver::pop(uint8_t*& ptr, size_t& len) {
    const auto hl = RTPHeader_trait::get_header_length() + J2KPayloadHeader_trait::get_header_length();

    if (cache.empty() || cache.is_main()) {
        if (unlikely(!(pos < num_packets))) { throw buffer_leak("out range"); }
        const auto& pkt = packets[pos++];
        ptr             = pkt.data + hl;
        if (unlikely(J2KPayloadHeader_trait::get_MH(pkt.data + RTPHeader_trait::get_header_length()))) {
            len = static_cast<size_t>(pkt.len - hl);
        } else {
            if (pos != num_packets) {
                if (unlikely(J2KPayloadHeader_trait::get_body_ORDB(pkt.data + RTPHeader_trait::get_header_length()))) { throw buffer_leak("ORDB"); }
                len = static_cast<size_t>(pkt.len - hl);
            } else {
                if (unlikely(!J2KPayloadHeader_trait::get_body_ORDB(pkt.data + RTPHeader_trait::get_header_length()))) { throw buffer_leak("ORDB"); }
                if (is_EOC) {
                    len = pkt.len;
                } else {
                    len = J2KPayloadHeader_trait::get_body_POS(pkt.data + RTPHeader_trait::get_header_length());
                }
            }
        }
    } else {
        ptr   = cache.data + hl + J2KPayloadHeader_trait::get_body_POS(cache.data + RTPHeader_trait::get_header_length());
        len   = static_cast<size_t>(cache.len - hl - J2KPayloadHeader_trait::get_body_POS(cache.data + RTPHeader_trait::get_header_length()));
        cache = {};
    }
}

int RTPReceiver::load_main_packet() {
    using namespace RTPHeader_trait;
    using namespace J2KPayloadHeader_trait;
    const auto hd = RTPHeader_trait::get_header_length();
    packets_.clear();

    const packet_t* pkt;

    while (true) {

        pkt = &packets_.push_back(buffer->pop());
        if (unlikely(pkt->size() == 1 && RTPHeader_trait::get_V(pkt->data()) != 0b10)) return this->FINISH;

        const auto m_seq    = get_extended_sequence_number(pkt->data());
        pre_sequence_number = m_seq;

        if (likely(get_MH(pkt->data() + hd))) { // メインヘッダ出現
            cache = {};
            pkt   = &packets_.push_back(buffer->pop());

            const auto b_seq = get_extended_sequence_number(pkt->data());

            if (likely((b_seq == m_seq + 1) || (m_seq == 0 && b_seq == 1) || (m_seq == ex_sequence_max || b_seq == 0))) {
                pre_sequence_number = b_seq;
                return this->MAIN_HEADER;
            }
            continue;
        }
    }
}