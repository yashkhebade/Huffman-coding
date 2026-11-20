// Self-contained test suite. No framework: compile and run.
#include <iostream>
#include <random>
#include "../src/codec.hpp"

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; std::cerr << "FAIL: " << msg << "\n"; } } while (0)

using Buf = std::vector<uint8_t>;

static void roundTrip(const std::string& name, const Buf& d) {
    for (auto m : {codec::Mode::Store, codec::Mode::Huffman, codec::Mode::Rle}) {
        try {
            CHECK(codec::decompress(codec::compressWith(d, m)) == d, name << " / " << codec::modeName(m));
        } catch (const std::exception& e) { CHECK(false, name << " / " << codec::modeName(m) << " threw: " << e.what()); }
    }
    auto best = codec::compressBest(d);
    CHECK(best.size() <= d.size() + codec::kHeaderSize, name << " auto mode exceeds STORE bound");
    CHECK(codec::decompress(best) == d, name << " / auto");
}

int main() {
    std::mt19937 rng(12345);
    Buf text; std::string s = "the quick brown fox jumps over the lazy dog. ";
    for (int i = 0; i < 2000; ++i) text.insert(text.end(), s.begin(), s.end());

    roundTrip("empty", {});
    roundTrip("one byte", {'x'});
    roundTrip("two same", {'a', 'a'});
    roundTrip("single symbol x100k", Buf(100000, 'z'));          // 1-symbol Huffman edge case + RLE runs > 255
    roundTrip("two symbols", {'a', 'b', 'a', 'a', 'b'});
    roundTrip("english-ish text", text);
    roundTrip("all 256 byte values", [] { Buf b; for (int i = 0; i < 256; ++i) b.push_back(uint8_t(i)); return b; }());
    roundTrip("NUL bytes + newlines", Buf{0, 0, 0, '\n', '\r', '\n', 0, 255, 255});
    { Buf r(50000); for (auto& b : r) b = uint8_t(rng()); roundTrip("random binary", r); }
    { Buf sk; for (int i = 0; i < 40000; ++i) sk.push_back(rng() % 10 ? 'e' : uint8_t(rng())); roundTrip("skewed", sk); }
    { Buf runs; for (int i = 0; i < 300; ++i) runs.insert(runs.end(), rng() % 700, uint8_t(i)); roundTrip("long runs", runs); }

    // Compression must actually compress where it should.
    CHECK(codec::compressWith(text, codec::Mode::Huffman).size() < text.size(), "huffman shrinks repetitive text");
    CHECK(codec::compressWith(Buf(100000, 'z'), codec::Mode::Rle).size() < 1000, "rle shrinks runs");

    // Huffman optimality sanity: output within 1 bit/symbol of entropy (+ header).
    {
        uint64_t f[256] = {}; for (auto b : text) ++f[b];
        double H = 0; for (auto c : f) if (c) { double p = double(c) / text.size(); H -= p * std::log2(p); }
        double bound = (H + 1.0) * text.size() / 8.0 + 600;
        CHECK(double(huff::encode(text).size()) <= bound, "huffman within H+1 bits/symbol");
    }

    // Corruption robustness: must throw or return exactly the original; never crash or hang.
    {
        auto good = codec::compressWith(text, codec::Mode::Huffman);
        int detected = 0, trials = 3000;
        for (int t = 0; t < trials; ++t) {
            auto bad = good; bad[rng() % bad.size()] ^= uint8_t(1u << (rng() % 8));
            try { auto r = codec::decompress(bad); CHECK(r == text, "silent corruption!"); }
            catch (const std::exception&) { ++detected; }
        }
        CHECK(detected > trials * 9 / 10, "most bit flips detected");
        for (size_t cut = 0; cut < good.size(); cut += 7) {
            Buf trunc(good.begin(), good.begin() + cut);
            try { codec::decompress(trunc); CHECK(false, "truncated file accepted"); } catch (const std::exception&) {}
        }
        Buf junk(100); for (auto& b : junk) b = uint8_t(rng());
        try { codec::decompress(junk); CHECK(false, "garbage accepted"); } catch (const std::exception&) {}
    }

    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
