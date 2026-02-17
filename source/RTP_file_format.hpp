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
    uint16_t get_packet(uint8_t* const& ptr) {
        char in[4] = {};
        file.read(in, 4);
        if (file.eof()) {
            std::cout << "reading file is EOF" << std::endl;
            return 0;
        }
        assert(in[0] == static_cast<char>(0xFF) && in[1] == static_cast<char>(0xFF));

        const uint16_t packet_size = (static_cast<uint8_t>(in[2]) << 8) + (static_cast<uint8_t>(in[3]));

        file.read(static_cast<char*>(static_cast<void*>(ptr)), packet_size);
        return packet_size;
    }

private:
    std::ifstream file;
};