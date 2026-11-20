// huffman.hpp - canonical Huffman coding over bytes (0..255)
//
// Payload layout:
//   u64 symbolCount | u16 numDistinct | numDistinct x (u8 symbol, u8 codeLen) | bitstream
//
// Only code LENGTHS are stored (not the tree, not frequencies). Both sides rebuild
// identical "canonical" codes from the lengths, which keeps the header small and
// makes decoding table-driven instead of pointer-chasing a tree.
#pragma once
#include <algorithm>
#include <array>
#include <functional>
#include <queue>
#include "bitio.hpp"
#include "bytes.hpp"

namespace huff {

constexpr int kMaxCodeLen = 60;  // codes live in uint64_t; real files never get near this

// Build the Huffman tree with a min-priority-queue and return each symbol's code length.
inline std::array<uint8_t, 256> codeLengths(const std::array<uint64_t, 256>& freq) {
    struct Node { int l, r; };
    std::vector<Node> nodes;          // leaves first, internal nodes after
    std::vector<int> leafSym;
    using Item = std::pair<uint64_t, int>;  // (weight, node index) -> index breaks ties deterministically
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;

    for (int s = 0; s < 256; ++s)
        if (freq[s]) { nodes.push_back({-1, -1}); leafSym.push_back(s); pq.push({freq[s], int(nodes.size()) - 1}); }

    std::array<uint8_t, 256> len{};
    if (nodes.empty()) return len;
    if (nodes.size() == 1) { len[leafSym[0]] = 1; return len; }  // edge case: 1 distinct symbol still needs 1 bit

    while (pq.size() > 1) {
        auto a = pq.top(); pq.pop();
        auto b = pq.top(); pq.pop();
        nodes.push_back({a.second, b.second});
        pq.push({a.first + b.first, int(nodes.size()) - 1});
    }
    // Children always have smaller indices than parents, so one reverse pass gives depths (no recursion).
    std::vector<int> depth(nodes.size(), 0);
    for (int i = int(nodes.size()) - 1; i >= 0; --i)
        if (nodes[i].l >= 0) { depth[nodes[i].l] = depth[i] + 1; depth[nodes[i].r] = depth[i] + 1; }
    for (size_t i = 0; i < leafSym.size(); ++i) {
        if (depth[i] > kMaxCodeLen) throw std::runtime_error("Huffman code too long");
        len[leafSym[i]] = uint8_t(depth[i]);
    }
    return len;
}

inline std::vector<uint8_t> encode(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> out;
    bytes::putU64(out, in.size());

    std::array<uint64_t, 256> freq{};
    for (uint8_t b : in) ++freq[b];
    auto len = codeLengths(freq);

    int numSyms = 0;
    for (int s = 0; s < 256; ++s) numSyms += len[s] != 0;
    bytes::putU16(out, uint16_t(numSyms));
    for (int s = 0; s < 256; ++s)
        if (len[s]) { out.push_back(uint8_t(s)); out.push_back(len[s]); }
    if (numSyms == 0) return out;

    // Canonical code assignment: sort by (length, symbol), count upward.
    std::vector<int> order;
    for (int s = 0; s < 256; ++s) if (len[s]) order.push_back(s);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return len[a] != len[b] ? len[a] < len[b] : a < b; });

    std::array<uint64_t, 256> code{};
    uint64_t c = 0; int prev = len[order[0]];
    for (size_t i = 0; i < order.size(); ++i) {
        int l = len[order[i]];
        if (i > 0) { ++c; c <<= (l - prev); prev = l; }
        code[order[i]] = c;
    }

    BitWriter bw(out);
    for (uint8_t b : in) bw.put(code[b], len[b]);
    bw.flush();
    return out;
}

inline std::vector<uint8_t> decode(const uint8_t* p, size_t n) {
    bytes::Reader rd(p, n);
    uint64_t total = rd.u64();
    int numSyms = rd.u16();
    if (numSyms > 256) throw std::runtime_error("bad symbol table");

    std::array<uint8_t, 256> len{};
    std::array<int, kMaxCodeLen + 2> count{};
    int maxLen = 0;
    for (int i = 0; i < numSyms; ++i) {
        int s = rd.u8(), l = rd.u8();
        if (l < 1 || l > kMaxCodeLen || len[s]) throw std::runtime_error("bad symbol table");
        len[s] = uint8_t(l); ++count[l]; maxLen = std::max(maxLen, l);
    }
    if (total == 0) return {};
    if (numSyms == 0) throw std::runtime_error("empty table for non-empty data");

    // Kraft check: a valid prefix code must be exactly complete (or the 1-symbol special case).
    long long left = 1;
    for (int l = 1; l <= maxLen; ++l) {
        left = (left << 1) - count[l];
        if (left < 0 || left > 256) throw std::runtime_error("invalid prefix code");
    }
    if (numSyms > 1 && left != 0) throw std::runtime_error("incomplete prefix code");

    // Canonical decode tables: first code and first symbol index per length.
    std::vector<int> sorted;
    for (int l = 1; l <= maxLen; ++l)
        for (int s = 0; s < 256; ++s) if (len[s] == l) sorted.push_back(s);
    std::array<long long, kMaxCodeLen + 2> first{}, offset{};
    long long fc = 0; int off = 0;
    for (int l = 1; l <= maxLen; ++l) {
        fc = (fc + (l > 1 ? count[l - 1] : 0)) << 1;
        first[l] = fc; offset[l] = off; off += count[l];
    }

    // Guard against absurd claimed sizes from corrupt headers: each symbol needs >= 1 bit.
    if (total > uint64_t(rd.remaining()) * 8) throw std::runtime_error("claimed size exceeds data");

    std::vector<uint8_t> out;
    out.reserve(size_t(total));
    BitReader br(rd.cur(), rd.remaining());
    for (uint64_t i = 0; i < total; ++i) {
        long long code = 0; int l = 0;
        for (;;) {
            code = (code << 1) | br.get();
            if (++l > maxLen) throw std::runtime_error("invalid code in stream");
            long long d = code - first[l];
            if (d >= 0 && d < count[l]) { out.push_back(uint8_t(sorted[size_t(offset[l] + d)])); break; }
        }
    }
    return out;
}

} // namespace huff
