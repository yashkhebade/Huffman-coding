// rle.hpp - byte-level Run-Length Encoding.  Format: repeated (count:1..255, byte).
// Binary-safe (no reserved escape byte). Worst case doubles the size; that is the
// honest behaviour for text with few runs, and the container's STORE fallback covers it.
#pragma once
#include <stdexcept>
#include <vector>
#include <cstdint>

namespace rle {

inline std::vector<uint8_t> encode(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < in.size()) {
        uint8_t b = in[i]; size_t run = 1;
        while (i + run < in.size() && in[i + run] == b && run < 255) ++run;
        out.push_back(uint8_t(run)); out.push_back(b);
        i += run;
    }
    return out;
}

inline std::vector<uint8_t> decode(const std::vector<uint8_t>& in) {
    if (in.size() % 2) throw std::runtime_error("bad RLE stream");
    std::vector<uint8_t> out;
    for (size_t i = 0; i < in.size(); i += 2) {
        if (in[i] == 0) throw std::runtime_error("bad RLE run length");
        out.insert(out.end(), in[i], in[i + 1]);
    }
    return out;
}

} // namespace rle
