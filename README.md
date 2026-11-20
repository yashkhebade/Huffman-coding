# huffz — Lossless Compression Suite (Huffman + RLE) in C++17

A production-grade, lossless byte-level data compression system featuring **Canonical Huffman Coding**, **Run-Length Encoding (RLE)**, and a **STORE fallback container format** with **CRC-32 integrity validation**. Includes a CLI tool, built-in HTTP server, and a modern in-browser Compression Studio.

---

## Features
- **Binary-Safe Byte Coder**: Operates on raw 8-bit bytes (`uint8_t`), seamlessly handling text, UTF-8, binary executables, images, and NUL bytes.
- **Canonical Huffman Trees**: Stores only symbol code lengths in the header ($\le 2$ bytes per symbol) instead of serialized trees or frequency tables.
- **Deterministic Tree Construction**: Uses a Min-Priority Queue (`std::priority_queue`) with tie-breaking on node indices.
- **Pigeonhole Fallback (`STORE`)**: Guarantees maximum expansion on random or high-entropy files is capped at strictly 17 bytes.
- **Lossless CRC-32 Verification**: Detects bit corruption and truncation automatically.
- **Three Interfaces**:
  1. **CLI Engine**: Fast command-line compression, decompression, and Shannon entropy benchmarking.
  2. **Embedded HTTP Server**: Built-in C++ socket server listening on `http://127.0.0.1:8080`.
  3. **In-Browser Compression Studio**: Modern glassmorphism web interface (`web/index.html`) running 100% locally via JavaScript typed arrays.

---

## Build & Run

### 1. Compile with C++14/C++17 (MinGW GCC, Clang, or MSVC)
```bash
# Build CLI engine:
g++ -std=c++14 -O2 -Wall -Wextra -Wpedantic -o huffz.exe src/main.cpp

# Build with embedded web server (requires -DHUFFZ_SERVER and Winsock on Windows):
g++ -std=c++17 -O2 -DHUFFZ_SERVER -o huffz.exe src/main.cpp -lws2_32

# Build test suite:
g++ -std=c++14 -O2 -Wall -Wextra -Wpedantic -o tests/tests.exe tests/tests.cpp
```

### 2. Run Commands
```bash
# Compress file (auto selects smallest among Store, Huffman, and RLE):
./huffz.exe c samples/natural_english.txt out.huffz --mode auto

# Decompress and verify CRC-32:
./huffz.exe d out.huffz restored.txt

# Run benchmark across sample files (shows Shannon entropy floor and ms timing):
./huffz.exe bench samples/*

# Run automated test suite (59 edge-case and fuzzing checks):
./tests/tests.exe

# Start local web server (when built with -DHUFFZ_SERVER):
./huffz.exe serve 8080
```

### 3. Web UI
Open `web/index.html` in any modern web browser to use the client-side Compression Studio with drag-and-drop file processing and live metric charts.

---

## Container Format (`HUFZ`)
```
 0               4        5             13         17
+----------------+--------+--------------+----------+-----------------------+
|  Magic "HUFZ"  |  Mode  | OrigSize u64 |  CRC-32  |        Payload        |
|    (4 bytes)   | (1 B)  |   (8 bytes)  |  (4 B)   |       (Variable)      |
+----------------+--------+--------------+----------+-----------------------+
```
- **Mode**: `0` = Store, `1` = Huffman, `2` = RLE.
- **Huffman Payload**: `symbolCount:u64 | numDistinct:u16 | (symbol:u8, len:u8)* | bitstream (MSB-first)`.

---

## Complexity Analysis ($n$ = input bytes, $k \le 256$ distinct symbols)
| Step | Time Complexity | Space Complexity |
|---|---|---|
| Frequency Count | $O(n)$ | $O(k)$ |
| Tree Construction (Heap) | $O(k \log k) \approx O(1)$ | $O(k)$ |
| Canonical Assignment | $O(k \log k) \approx O(1)$ | $O(k)$ |
| Bitstream Encoding | $O(n)$ | $O(n)$ |
| Bitstream Decoding | $O(n \cdot \bar{L})$ | $O(n)$ |
| CRC-32 Integrity Check | $O(n)$ | $O(1)$ |

Overall time complexity is strictly **linear $O(n)$**.
