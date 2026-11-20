// codec.hpp - container format + mode selection
//
//  offset  size  field
//  0       4     magic "HUFZ"
//  4       1     mode (0=STORE fallback, 1=HUFFMAN, 2=RLE)
//  5       8     original size (u64 LE)
//  13      4     CRC-32 of original data
//  17      ...   payload
#pragma once
#include <string>
#include "bytes.hpp"
#include "huffman.hpp"
#include "rle.hpp"

namespace codec {

enum class Mode : uint8_t { Store = 0, Huffman = 1, Rle = 2 };
constexpr size_t kHeaderSize = 17;

inline const char* modeName(Mode m) {
    switch (m) {
        case Mode::Store: return "store";
        case Mode::Huffman: return "huffman";
        case Mode::Rle: return "rle";
    }
    return "?";
}

inline bool parseMode(const std::string& s, Mode& out) {
    if (s == "store") { out = Mode::Store; return true; }
    if (s == "huffman" || s == "huff") { out = Mode::Huffman; return true; }
    if (s == "rle") { out = Mode::Rle; return true; }
    return false;
}

inline std::vector<uint8_t> compressWith(const std::vector<uint8_t>& in, Mode m) {
    std::vector<uint8_t> out = {'H', 'U', 'F', 'Z', uint8_t(m)};
    bytes::putU64(out, in.size());
    bytes::putU32(out, bytes::crc32(in));
    std::vector<uint8_t> payload;
    switch (m) {
        case Mode::Store: payload = in; break;
        case Mode::Huffman: payload = huff::encode(in); break;
        case Mode::Rle: payload = rle::encode(in); break;
    }
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

// Auto mode: try everything, keep the smallest. STORE guarantees worst-case overhead = 17 bytes.
inline std::vector<uint8_t> compressBest(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> best;
    for (Mode m : {Mode::Store, Mode::Huffman, Mode::Rle}) {
        auto c = compressWith(in, m);
        if (best.empty() || c.size() < best.size()) best = std::move(c);
    }
    return best;
}

inline std::vector<uint8_t> decompress(const std::vector<uint8_t>& in) {
    bytes::Reader rd(in.data(), in.size());
    if (rd.u8() != 'H' || rd.u8() != 'U' || rd.u8() != 'F' || rd.u8() != 'Z')
        throw std::runtime_error("not a HUFZ file (bad magic)");
    uint8_t mode = rd.u8();
    uint64_t origSize = rd.u64();
    uint32_t crc = rd.u32();

    std::vector<uint8_t> out;
    switch (mode) {
        case 0: out.assign(rd.cur(), rd.cur() + rd.remaining()); break;
        case 1: out = huff::decode(rd.cur(), rd.remaining()); break;
        case 2: out = rle::decode(std::vector<uint8_t>(rd.cur(), rd.cur() + rd.remaining())); break;
        default: throw std::runtime_error("unknown mode");
    }
    if (out.size() != origSize) throw std::runtime_error("size mismatch after decompression");
    if (bytes::crc32(out) != crc) throw std::runtime_error("CRC mismatch: data corrupted");
    return out;
}

inline Mode modeOf(const std::vector<uint8_t>& c) { return c.size() > 4 ? Mode(c[4]) : Mode::Store; }

} // namespace codec
