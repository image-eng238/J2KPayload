#pragma once
#include <iostream>
#include <fstream>
#include <memory>
#include <cassert>

class RTPFile {
public:
    RTPFile() = default;
    RTPFile(const char* file_path) : RTPFile() { this->open(file_path); }
    ~RTPFile() { file.close(); }

    void open(const char* file_path) {
        file.open(file_path);
        if (!file.is_open()) {
            std::cout << "can't open the file: '" << file_path << "'" << std::endl;
            exit(1);
        }
    }

    void reopen(const char* file_path) {
        file.close();
        this->open(file_path);
    }

    template <typename T>
    uint16_t get_packet(std::unique_ptr<T[]>& ptr) {
        char in[4] = {};
        file.read(in, 4);
        if (file.eof()) {
            std::cout << "reading file is EOF" << std::endl;
            return 0;
        }
        assert(in[0] == static_cast<char>(0xFF) && in[1] == static_cast<char>(0xFF));

        const uint16_t packet_size = (static_cast<uint8_t>(in[2]) << 8) + (static_cast<uint8_t>(in[3]));

        ptr = std::make_unique<T[]>(packet_size);
        file.read(static_cast<char*>(static_cast<void*>(ptr.get())), packet_size);
        return packet_size;
    }
    uint16_t get_packet(uint8_t* const& ptr, const uint32_t& diff = 0) {
        char in[4] = {};
        file.read(in, 4);
        if (file.eof()) {
            std::cout << "reading file is EOF" << std::endl;
            return 0;
        }
        assert(in[0] == static_cast<char>(0xFF) && in[1] == static_cast<char>(0xFF));

        const uint16_t packet_size = (static_cast<uint8_t>(in[2]) << 8) | (static_cast<uint8_t>(in[3]));

        file.read(static_cast<char*>(static_cast<void*>(ptr)), packet_size);

        if (packet_size == 0) return 0;

        // シーケンス番号を進める
        const uint16_t rtp_seq = static_cast<uint16_t>((ptr[2] << 8) | ptr[3]);
        uint32_t base_seq      = static_cast<uint32_t>((ptr[RTP_PACKET_HEADER_LENGTH + 3] << 16) | rtp_seq);

        base_seq += diff;

        ptr[2] = (base_seq >> 8) & 0xFF;
        ptr[3] = base_seq & 0xFF;

        ptr[RTP_PACKET_HEADER_LENGTH + 3] = (base_seq >> 16) & 0xFF;

        return packet_size;
    }

private:
    static constexpr size_t RTP_PACKET_HEADER_LENGTH  = 12;
    static constexpr size_t J2K_PAYLOAD_HEADER_LENGTH = 8;
    std::ifstream file;
};