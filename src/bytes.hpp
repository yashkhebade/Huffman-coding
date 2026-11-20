// bytes.hpp - little-endian helpers, bounds-checked reader, CRC-32
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace bytes {

inline void putU16(std::vector<uint8_t>& o, uint16_t v) {
    o.push_back(uint8_t(v)); o.push_back(uint8_t(v >> 8));
}
inline void putU32(std::vector<uint8_t>& o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o.push_back(uint8_t(v >> (8 * i)));
}
inline void putU64(std::vector<uint8_t>& o, uint64_t v) {
    for (int i = 0; i < 8; ++i) o.push_back(uint8_t(v >> (8 * i)));
}

// Bounds-checked sequential reader. Corrupt input throws, never reads OOB.
class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    uint8_t u8() { need(1); return p_[pos_++]; }
    uint16_t u16() { need(2); uint16_t v = p_[pos_] | (p_[pos_ + 1] << 8); pos_ += 2; return v; }
    uint32_t u32() { need(4); uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(p_[pos_ + i]) << (8 * i); pos_ += 4; return v; }
    uint64_t u64() { need(8); uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= uint64_t(p_[pos_ + i]) << (8 * i); pos_ += 8; return v; }
    const uint8_t* cur() const { return p_ + pos_; }
    size_t remaining() const { return n_ - pos_; }
private:
    void need(size_t k) const { if (n_ - pos_ < k) throw std::runtime_error("truncated data"); }
    const uint8_t* p_; size_t n_; size_t pos_ = 0;
};

inline uint32_t crc32(const std::vector<uint8_t>& d) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : d) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

} // namespace bytes
