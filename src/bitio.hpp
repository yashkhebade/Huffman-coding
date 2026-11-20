// bitio.hpp - MSB-first bit-level writer/reader
#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

class BitWriter {
public:
    explicit BitWriter(std::vector<uint8_t>& out) : out_(out) {}
    // Write the low `len` bits of `code`, most significant bit first.
    void put(uint64_t code, int len) {
        for (int i = len - 1; i >= 0; --i) {
            cur_ = uint8_t((cur_ << 1) | ((code >> i) & 1));
            if (++n_ == 8) { out_.push_back(cur_); cur_ = 0; n_ = 0; }
        }
    }
    // Pad final partial byte with zeros. Decoder knows symbol count, so padding is harmless.
    void flush() {
        if (n_) { out_.push_back(uint8_t(cur_ << (8 - n_))); cur_ = 0; n_ = 0; }
    }
private:
    std::vector<uint8_t>& out_;
    uint8_t cur_ = 0; int n_ = 0;
};

class BitReader {
public:
    BitReader(const uint8_t* p, size_t nbytes) : p_(p), nbits_(nbytes * 8) {}
    int get() {
        if (pos_ >= nbits_) throw std::runtime_error("bitstream exhausted (corrupt data)");
        int b = (p_[pos_ >> 3] >> (7 - (pos_ & 7))) & 1;
        ++pos_;
        return b;
    }
private:
    const uint8_t* p_; size_t nbits_; size_t pos_ = 0;
};
