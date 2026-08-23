#pragma once
#include "UDP.hpp"
#include "RTP_header.hpp"
#include "packet_t.hpp"

#include <filesystem>
#include <thread>
#include <vector>
#include <string_view>

class RTP_file {
public:
    RTP_file() : codestream{}, packets{}, siz{}, frames{} {};
    RTP_file(const std::string_view file_path) : RTP_file{} {
        if (!load(file_path)) {
            fprintf(stderr, "can`t load file: '%s'\n", file_path.data());
            exit(1);
        }
    }
    bool load(const std::string_view f, std::uintmax_t max_alloc_size = UINTMAX_MAX) {
        if (!std::filesystem::exists(f)) return false;
        siz        = std::min(std::filesystem::file_size(f), max_alloc_size);
        codestream = std::make_unique<uint8_t[]>(siz);
        FILE* fp   = fopen(f.data(), "r");
        if (fp == nullptr) {
            return false;
        }
        const auto s = fread(codestream.get(), sizeof(uint8_t), siz, fp);
        fclose(fp);
        if (s != siz) {
            return false;
        }

        const size_t hd = 4;
        for (uintmax_t i = 0; i < siz;) {
            uint8_t* const pktdata = codestream.get() + i;
            if (pktdata[0] != 0xFF || pktdata[1] != 0xFF) {
                return false;
            }
            const size_t pktsiz = (pktdata[2] << 8) | pktdata[3];
            if (i += pktsiz + hd; i <= siz) {
                packets.push_back(packet_t{pktdata + hd, pktsiz});
                if (RTPHeader_trait::get_M(packets.back().data())) ++frames;
            } else {
                break;
            }
        }
        return true;
    }
    size_t pickup_frame(size_t f) const {
        if (f == 0) return 0;
        if (f > frames) f = frames;
        size_t i = 0, j = 0;
        for (; i != f - 1; ++i, ++j)
            for (; !RTPHeader_trait::get_M(packets[j].data()); ++j);
        return j;
    }
    auto get_pkt(size_t i) const { return packets[i]; }
    auto& front() { return packets.front(); }
    auto& back() { return packets.back(); }
    auto num_packet() const { return packets.size(); }
    auto num_frame() const { return frames; }

private:
    std::unique_ptr<uint8_t[]> codestream;
    std::vector<packet_t> packets;
    uintmax_t siz;
    size_t frames;
};

class cli_parser {
public:
    cli_parser() : ignore_count{}, input{}, opt_number{}, opt_character{}, is_active{false} {}
    cli_parser(bool active) : cli_parser{} { is_active = active; }

    bool read_line(const char* const prefix = nullptr) {
        if (!is_active) return false;

        if (ignore_count != 0) {
            --ignore_count;
            return false;
        }

        if (prefix != nullptr) {
            printf("%s> ", prefix);
        } else {
            printf("> ");
        }
        int c = 0;
        input.clear();
        while ((c = getchar()) != '\n') {
            if (c == -1) { return false; }
            input.push_back(c);
        }
        if (input.empty()) {
            opt_character = 's';
            opt_number    = 1;
            return true;
        }

        while (true) {
            try {
                opt_character     = input.at(input.find_first_not_of(' '));
                const auto numpos = input.find_first_of("0123456789", 0);
                if (numpos == 0) {
                    opt_character = 's';
                }
                if (numpos != std::string::npos) {
                    std::string_view str{input.data() + numpos};
                    opt_number = std::stoi(std::string{str});
                } else {
                    opt_number = 1;
                }
                return true;
            } catch (std::runtime_error& e) {
                std::cerr << e.what();
            } catch (std::exception& e) {
                std::cerr << e.what();
                exit(1);
            }
        }
    }

    void set_ignore(size_t n) { ignore_count = n - 1; }

    int optn() const { return opt_number; }
    char optc() const { return opt_character; }
    bool active() const { return is_active; }

private:
    size_t ignore_count;
    std::string input;
    int opt_number;
    char opt_character;
    bool is_active;
};

class packet_os {
public:
    packet_os() : tp{}, base_tp{}, exseq{}, base_exseq{}, ptp{}, base_ptp{} {};
    packet_os(const packet_t& pkt) : packet_os{} { set_base(pkt); }

    void set_base(const packet_t& pkt) {
        tp = base_tp = RTPHeader_trait::get_timestamp(pkt.data());
        exseq = base_exseq = J2KPayloadHeader_trait::get_extended_sequence_number(pkt.data());
        ptp = base_ptp = J2KPayloadHeader_trait::get_PTSTAMP(pkt.data() + RTPHeader_trait::length);
    }

    void advance_tp(packet_t& pkt, uint32_t n) {
        RTPHeader_trait::set_timestamp(pkt.data(), tp);
        if (RTPHeader_trait::get_M(pkt.data())) {
            tp = tp.get() + n;
        }
    }
    void advance_seq(packet_t& pkt) { J2KPayloadHeader_trait::set_extended_sequence_number(pkt.data(), exseq++); }
    void advance_ptp(packet_t& pkt, uint16_t n) {
        J2KPayloadHeader_trait::set_PTSTAMP(pkt.data() + RTPHeader_trait::length, ptp);
        ptp = ptp.get() + n;
    }

    void set_tp(uint32_t n) { tp = n; }
    void set_exseq(uint32_t n) { exseq = n; }
    void set_ptp(uint32_t n) { ptp = n; }

    void to_base() {
        tp    = base_tp;
        exseq = base_exseq;
        ptp   = base_ptp;
    }

    void write_tp(packet_t& pkt) { RTPHeader_trait::set_timestamp(pkt.data(), tp); }
    void write_exseq(packet_t& pkt) { J2KPayloadHeader_trait::set_extended_sequence_number(pkt.data(), exseq); }
    void write_ptp(packet_t& pkt) { J2KPayloadHeader_trait::set_PTSTAMP(pkt.data() + RTPHeader_trait::length, ptp); }
    void write(packet_t& pkt) {
        write_tp(pkt);
        write_exseq(pkt);
        write_ptp(pkt);
    }

private:
    rtptimestamp_t tp;
    uint32_t base_tp;
    exsequence_t exseq;
    uint32_t base_exseq;
    j2kptstamp_t ptp;
    uint16_t base_ptp;
};

class packet_sender {
public:
    packet_sender(std::string_view addr, uint16_t port)
        : udp{addr.data(), port}, send_call{}, sent_f_packet{}, lost_packet{}, ignore_count{}, sent_frame{}, out_frame{}, sum_avg{}, data_fps{}, adv_sleep_v{}, sleep_v{} {}

    bool send(const packet_t& pkt) {
        ++send_call;
        if (ignore_count != 0) {
            --ignore_count;
            ++lost_packet;
        } else {
            if (udp.send(pkt.ptr, pkt.len) == -1) {
                perror("sendto");
                return false;
            }
        }
        if (RTPHeader_trait::get_M(pkt.data())) {
            ++sent_frame;
            sent_f_packet = send_call;
            std::this_thread::sleep_until(sleep_v += adv_sleep_v);
            if (sent_frame % out_frame == 0) {
                const auto now_time = std::chrono::steady_clock::now();
                const auto ms       = std::chrono::duration_cast<std::chrono::microseconds>(now_time - prev_time);
                const auto fps      = (1 / (static_cast<float>(ms.count()) / out_frame)) * 1000000;
                sum_avg += fps;
                printf("sent_frame: %ld, avg: %.6ffps\n", sent_frame, fps);
                prev_time = now_time;
            }
        }
        return true;
    }

    void set_ignore(size_t n) { ignore_count = n; }
    void set_clock(size_t t) {
        data_fps    = static_cast<double>(J2KPayloadHeader_trait::media_clock_Hz) / t;
        adv_sleep_v = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(static_cast<double>(t) / J2KPayloadHeader_trait::media_clock_Hz)
        );
        out_frame = static_cast<size_t>(J2KPayloadHeader_trait::media_clock_Hz / static_cast<double>(t) + 0.5);
        prev_time = sleep_v = std::chrono::steady_clock::now();
    }
    void set_sleep_v() { prev_time = sleep_v = std::chrono::steady_clock::now(); }
    void clear() {
        send_call     = 0;
        sent_f_packet = 0;
        lost_packet   = 0;
        sent_frame    = 0;
        sum_avg       = 0;
    }

    size_t get_call() const { return send_call; }
    auto get_sent_frame() const { return sent_frame; }
    auto get_sent_f_packet() const { return sent_f_packet; }
    auto get_fpkt() const { return send_call - sent_f_packet; }

    void print_result() {
        printf("========================================\n");
        printf("fps based on data: %lffps ~= %ldfps\n", data_fps, out_frame);
        printf("average fps: %lffps\n", sum_avg / (sent_frame / out_frame));
        printf("lost packets: %ld\n", lost_packet);
        printf("sent packets: %ld\n", send_call - lost_packet);
        printf("sent frames: %ld\n", sent_frame);
    }

private:
    UDPSender udp;
    size_t send_call;
    size_t sent_f_packet;
    size_t lost_packet;
    size_t ignore_count;
    size_t sent_frame;
    size_t out_frame;
    double sum_avg;
    double data_fps;
    std::chrono::steady_clock::duration adv_sleep_v;
    std::chrono::steady_clock::time_point sleep_v;
    std::chrono::steady_clock::time_point prev_time;
};