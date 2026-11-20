# Text Compression Tool using Huffman Coding
## Comprehensive Data Structures & Algorithms (DSA) Implementation & Viva Guide

---

## 1. Problem Statement & University Objectives

### Problem Statement
Text files contain a significant amount of redundant information that can be encoded using fewer bits. When stored or transmitted in uncompressed raw form, they consume unnecessary storage capacity and network bandwidth. 
This project implements a **lossless text compression system** based on **Huffman Coding** (with **Run-Length Encoding (RLE)** as an optional technique for highly repetitive text). The compressed file (`.huff` or `.huffz`) can be decompressed back into the exact original file with **zero data loss, modification, or corruption**.

### Mapping to the 10 Project Objectives

| # | University Objective | Implementation in this Project |
|---|---|---|
| **1** | **Design & Implement Huffman Coding** | Implemented in `src/huffman.hpp`. Computes symbol frequencies, constructs a binary prefix tree, and assigns variable-length bit codes. |
| **2** | **Achieve Lossless Compression** | Operates on unsigned 8-bit bytes (`uint8_t`), preserving every byte (including NUL bytes, UTF-8 multibyte sequences, and newlines) without modification. |
| **3** | **Implement Decompression** | Implemented in `huff::decode()`. Reads header metadata, reconstructs canonical codes, and decodes the bitstream back to the original text. |
| **4** | **Reduce Storage Requirements** | Evaluated on multiple files; benchmarks compare size against the theoretical Order-0 Shannon Entropy limit, saving 40%–98% space. |
| **5** | **Compare Different Input Types** | Tested across 5 distinct datasets in `samples/`: natural language (`natural_english.txt`), repetitive text (`repetitive.txt`), source code (`source_code.cpp`), log files (`log_like.txt`), and random binary (`random.bin`). |
| **6** | **Implement Run-Length Encoding (RLE)** | Implemented in `src/rle.hpp`. Encodes consecutive identical character runs and provides an empirical comparison with Huffman Coding. |
| **7** | **Measure Compression Performance** | CLI (`huffz bench`) and GUI calculate exact compression ratio ($C_{\text{size}} / O_{\text{size}}$), percentage of space saved ($[1 - \text{ratio}] \times 100\%$), and execution time in milliseconds. |
| **8** | **Develop a User-Friendly Interface** | Provides three complementary interfaces: 1) High-performance CLI (`huffz.exe`), 2) Python Tkinter desktop GUI (`gui.py`), and 3) In-browser Web Studio (`web/index.html`). |
| **9** | **Analyse Algorithm Complexity** | Complete mathematical proof demonstrating linear $O(n)$ time and $O(n)$ space complexity. |
| **10** | **Verify Data Integrity** | Built-in verification command (`huffz verify original.txt restored.txt`) checking 32-bit CRC-32 checksums, file sizes, and bit-for-bit equality. |

---

## 2. System Architecture & Encoding/Decoding Flow

```
+-----------------------------------------------------------------------------------------+
|                              COMPRESSION PIPELINE (ENCODING)                            |
+-----------------------------------------------------------------------------------------+
  [Input File (.txt)]
          │
          ▼
  1. Frequency Analysis: Direct-address array counts byte occurrences in O(n)
          │
          ▼
  2. Build Min-Heap: Insert k unique symbol leaves into std::priority_queue in O(k log k)
          │
          ▼
  3. Greedy Tree Construction: Repeatedly extract 2 smallest nodes, merge into parent
          │
          ▼
  4. Canonical Code Derivation: Sort by (length, symbol); assign sequential binary codes
          │
          ▼
  5. Bitstream Packing (BitWriter): Map characters to bit codes, pack into 8-bit bytes
          │
          ▼
  [Compressed Container (.huff)] -> Magic 'HUFZ' (4B) | Mode (1B) | OrigSize (8B) | CRC-32 (4B)

+-----------------------------------------------------------------------------------------+
|                             DECOMPRESSION PIPELINE (DECODING)                           |
+-----------------------------------------------------------------------------------------+
  [Compressed Container (.huff)]
          │
          ▼
  1. Header Validation: Check magic 'HUFZ', mode, original size, and original CRC-32
          │
          ▼
  2. Read Symbol Table: Extract (symbol, code_length) pairs; verify Kraft's Inequality
          │
          ▼
  3. Reconstruct Canonical Decode Table: Calculate first code and symbol offset per length
          │
          ▼
  4. Bitstream Extraction (BitReader): Read bits MSB-first; match prefix code -> write byte
          │
          ▼
  5. Integrity Check: Verify restored byte count == OrigSize and CRC-32(restored) == Header CRC
          │
          ▼
  [Restored Output (.txt)] (100% Bit-for-Bit Identical to Original)
```

---

## 3. Exhaustive Data Structure & Algorithm Deep-Dive

### A. Direct-Address Frequency Table (Hash Array)
- **Time Complexity:** $O(n)$ | **Space Complexity:** $O(1)$ (Fixed 256 entries)
- Instead of using a generic hash map (`std::unordered_map`) which incurs hash collisions, dynamic node allocations, and pointer-chasing latency, we allocate a flat array: `std::array<uint64_t, 256> freq{}`.
- Because characters are 8-bit unsigned bytes (`0..255`), the byte value itself serves directly as the array index.
- Every increment operation `++freq[byte]` runs in strictly **$O(1)$** time with 100% CPU cache locality.

### B. Min-Priority Queue (Binary Min-Heap)
- **Time Complexity:** $O(k \log k)$ | **Space Complexity:** $O(k)$ ($k \le 256$)
- Implemented via `std::priority_queue<Item, std::vector<Item>, std::greater<Item>>`.
- A binary min-heap guarantees that the minimum element is always at the root. Finding the minimum is $O(1)$; extracting it and restoring the heap property takes $O(\log k)$.
- **Array indexing relationship:** For a node at index $i$:
  - Left Child: $2i + 1$
  - Right Child: $2i + 2$
  - Parent: $\lfloor (i - 1) / 2 \rfloor$
- **Why Min-Heap instead of a sorted array?** If we kept a sorted array, inserting the combined node after each merge would take $O(k)$ linear insertion or $O(k \log k)$ re-sorting. Across $k$ merges, that would require $O(k^2)$ time. A binary heap achieves this in strictly $O(k \log k)$.

### C. Binary Prefix-Free Huffman Tree (Greedy Strategy)
- **Time Complexity:** $O(k \log k)$ | **Space Complexity:** $O(k)$
- Huffman coding is a classic **Greedy Algorithm**. At every step, it makes the locally optimal choice: it extracts the two subtrees with the lowest frequencies and merges them under a parent whose frequency is their sum. Repeating this until one node remains guarantees the globally minimum weighted path length.
- **The Prefix-Free Property:** No code word is a prefix (beginning) of any other code word. For example, if `'E'` is assigned `01`, no other symbol can begin with `01` (such as `011`). This allows the decompressor to parse a continuous bitstream unambiguously on-the-fly without needing delimiters or spaces.

### D. Canonical Huffman Coding
- **Time Complexity:** $O(k \log k)$ | **Space Complexity:** $O(k)$
- Textbook Huffman algorithms store the entire serialized binary tree or all 256 frequency integers in the file header, wasting 500 to 1,024 bytes.
- **Canonical Huffman** stores **only the code length** of each character ($\le 2$ bytes per symbol: symbol + length).
- **Rules:**
  1. Shorter codes have lower numerical values than longer codes.
  2. Codes of identical length are ordered sequentially by their symbol byte value.
- Both compressor and decompressor generate identical binary codes by sorting by length and incrementing a numerical counter.

### E. Bit Manipulation & Bit-Level File Handling (BitWriter & BitReader)
- **Time Complexity:** $O(n)$ | **Space Complexity:** $O(1)$ buffer
- Operating systems can only read and write whole 8-bit bytes. Because Huffman codes have variable bit lengths (e.g., 3 bits, 7 bits), `BitWriter` and `BitReader` manage bit packing in `src/bitio.hpp`.
- `BitWriter` buffers incoming bits into an 8-bit accumulator (`uint8_t cur_`) using bitwise shifts and OR: `cur_ = (cur_ << 1) | bit`. Once 8 bits accumulate, the complete byte is written to memory. At the end, `flush()` pads the last byte with zeros.

### F. Run-Length Encoding (RLE) (Two-Pointer Linear Scan)
- **Time Complexity:** $O(n)$ | **Space Complexity:** $O(n)$
- RLE uses two pointers to count consecutive repetitions of identical bytes (runs up to 255).
- Output format: pairs of `[run_count: u8, byte: u8]`.
- For repetitive files, RLE achieves up to 98% space reduction in milliseconds. For varied text, the `STORE` fallback ensures files never expand unpredictably.

### G. Kraft's Inequality Mathematical Validation
- **Formula:** $\sum_{i=1}^{k} 2^{-l_i} \le 1.0$
- In `src/huffman.hpp`, before decoding, we compute this sum. If the sum exceeds 1 or capacity remains unallocated for $k > 1$, the file header is flagged as corrupt immediately, preventing malicious headers from triggering infinite loops or buffer overruns.

### H. CRC-32 IEEE 802.3 Integrity Checksum
- **Time Complexity:** $O(n)$ | **Space Complexity:** $O(1)$
- Treats the file as a polynomial and divides it by `0xEDB88320` using a fast 256-entry lookup table. Decompression recomputes this 32-bit checksum and validates it against the header value, guaranteeing bit-for-bit lossless accuracy.

---

## 4. Word-by-Word & Letter-by-Letter Step-by-Step Trace

Let us trace the concrete input string: **`"ABRACADABRA"`** (11 characters, 88 bits uncompressed).

### Step 1: Frequency Counting
- `'A'`: 5 occurrences (45.5%)
- `'B'`: 2 occurrences (18.2%)
- `'R'`: 2 occurrences (18.2%)
- `'C'`: 1 occurrence (9.1%)
- `'D'`: 1 occurrence (9.1%)
- Total symbols $n = 11$, Distinct alphabet $k = 5$.

### Step 2: Min-Heap Initialization
Leaf nodes pushed into Min-Heap (sorted by weight):
`Heap = [ ('C', 1), ('D', 1), ('B', 2), ('R', 2), ('A', 5) ]`

### Step 3: Greedy Tree Merging
1. **Merge 1:** Pop `('C', 1)` and `('D', 1)` $\rightarrow$ Create parent `N1` (wt 2). Push `N1`.
   `Heap = [ ('B', 2), ('R', 2), (N1, 2), ('A', 5) ]`
2. **Merge 2:** Pop `('B', 2)` and `('R', 2)` $\rightarrow$ Create parent `N2` (wt 4). Push `N2`.
   `Heap = [ (N1, 2), (N2, 4), ('A', 5) ]`
3. **Merge 3:** Pop `(N1, 2)` and `(N2, 4)` $\rightarrow$ Create parent `N3` (wt 6). Push `N3`.
   `Heap = [ ('A', 5), (N3, 6) ]`
4. **Merge 4:** Pop `('A', 5)` and `(N3, 6)` $\rightarrow$ Create `Root` (wt 11). Tree complete!

### Step 4: Tree Depths & Canonical Codes
Depths derived from tree:
- `'A'` $\rightarrow$ Length = 1 bit
- `'B'` $\rightarrow$ Length = 3 bits
- `'R'` $\rightarrow$ Length = 3 bits
- `'C'` $\rightarrow$ Length = 4 bits
- `'D'` $\rightarrow$ Length = 4 bits

Canonical Assignment (Sorted by length then symbol):
- `'A'` (len 1) $\rightarrow$ `0`
- `'B'` (len 3) $\rightarrow$ `100`
- `'R'` (len 3) $\rightarrow$ `101`
- `'C'` (len 4) $\rightarrow$ `1100`
- `'D'` (len 4) $\rightarrow$ `1101`

### Step 5: Letter-by-Letter Bitstream Generation
- Letter 1: `'A'` $\rightarrow$ `0`
- Letter 2: `'B'` $\rightarrow$ `100`
- Letter 3: `'R'` $\rightarrow$ `101`
- Letter 4: `'A'` $\rightarrow$ `0`
- Letter 5: `'C'` $\rightarrow$ `1100`
- Letter 6: `'A'` $\rightarrow$ `0`
- Letter 7: `'D'` $\rightarrow$ `1101`
- Letter 8: `'A'` $\rightarrow$ `0`
- Letter 9: `'B'` $\rightarrow$ `100`
- Letter 10: `'R'` $\rightarrow$ `101`
- Letter 11: `'A'` $\rightarrow$ `0`
- **Output bitstream:** `0 100 101 0 1100 0 1101 0 100 101 0`
- **Total Compressed Bits:** **23 bits** (Original was 88 bits) $\rightarrow$ **73.8% space saved!**

### Step 6: Byte Packing & Decompression
- Bits are packed into 8-bit bytes by `BitWriter`.
- `BitReader` reads bits sequentially. When the bit sequence matches a prefix code, the corresponding character is emitted. Output matches `"ABRACADABRA"` bit-for-bit.

---

## 5. Block-by-Block Source Code Walkthrough

### `src/huffman.hpp`
- **`codeLengths()` (Lines 22–51):** 
  Constructs tree nodes in a linear `std::vector<Node>`, initializes `std::priority_queue` with `std::greater`, and performs $k-1$ greedy merges. Computes depths non-recursively in reverse order ($O(k)$ time, zero stack overflow risk).
- **`encode()` (Lines 53–85):** 
  Serializes the symbol count and symbol table, calculates Canonical codes, and streams bits via `BitWriter`.
- **`decode()` (Lines 87–139):** 
  Parses the header, evaluates Kraft's inequality, builds canonical fast-lookup arrays (`first` code and `offset` per length), and decodes symbols bit-by-bit.

### `src/bitio.hpp`
- **`BitWriter`:** Accumulates bits into `uint8_t cur_` using `(cur_ << 1) | bit`. Flushes whole bytes to output when 8 bits accumulate.
- **`BitReader`:** Extracts individual bits using bitwise masking and shifting: `(p_[pos_ >> 3] >> (7 - (pos_ & 7))) & 1`.

### `src/codec.hpp`
- **`kHeaderSize = 17`:** Encapsulates the 17-byte `HUFZ` header: Magic (4B), Mode (1B), Original Size (8B), CRC-32 (4B).
- **`compressBest()`:** Automatically executes Store, Huffman, and RLE, selecting the smallest output.

### `src/rle.hpp`
- **`encode()` & `decode()`:** Two-pointer run counting up to 255 bytes.

### `src/bytes.hpp`
- **`crc32()`:** Fast table-driven 32-bit polynomial checksum generator.

### `src/main.cpp`
- **CLI Commands:** `c` (compress), `d` (decompress), `bench` (benchmark), `verify` (integrity check), `demo` (DSA trace), and `serve` (HTTP server).

---

## 6. Algorithm Complexity Analysis

| Stage | Data Structure | Time Complexity | Space Complexity |
|---|---|---|---|
| Frequency Count | Direct-Address Array | $O(n)$ | $O(k) \le 256$ entries |
| Tree Building | Min-Priority Queue | $O(k \log k) \approx O(1)$ | $O(k)$ nodes |
| Depth Calculation | Linear vector sweep | $O(k) \approx O(1)$ | $O(k)$ depths |
| Canonical Assignment | `std::sort` + Bit-shifts | $O(k \log k) \approx O(1)$ | $O(k)$ codes |
| Bitstream Encoding | `BitWriter` (8-bit buffer) | $O(n)$ | $O(n)$ output buffer |
| Bitstream Decoding | `BitReader` + Canonical Table | $O(n \cdot \bar{L})$ ($\bar{L} \le 8$) | $O(n)$ output buffer |
| CRC-32 Checksum | 256-entry lookup table | $O(n)$ | $O(1)$ register |
| **Overall Pipeline** | **Complete Compression Tool** | **$O(n)$ Linear Time** | **$O(n)$ Memory Buffer** |

Since $k \le 256$ is a fixed constant, $O(k \log k) = O(256 \log 256) = O(1)$. Overall execution is strictly **linear $O(n)$**.

---

## 7. Empirical Results Across Different Input Types

| File / Dataset | Original Size | Compressed Size | Ratio | Space Saved | Latency | Best Method |
|---|---|---|---|---|---|---|
| `natural_english.txt` | 35,149 B | 20,431 B | 0.581 | 41.9% | 2.0 ms | **Huffman** |
| `repetitive.txt` | 100,645 B | 2,017 B | 0.020 | 98.0% | 0.4 ms | **RLE** |
| `source_code.cpp` | 2,709 B | 1,780 B | 0.657 | 34.3% | 0.0 ms | **Huffman** |
| `log_like.txt` | 225,000 B | 116,420 B | 0.517 | 48.3% | 5.1 ms | **Huffman** |
| `random.bin` (Max Entropy) | 100,000 B | 100,017 B | 1.000 | 0.0% | 3.2 ms | **Store (Pigeonhole bound)** |

---

## 8. Top 15 Viva Voce Questions & Model Answers

1. **What makes Huffman Coding a Greedy Algorithm?**
   At each step, it merges the two lowest-frequency subtrees. This local greedy choice mathematically guarantees the globally minimal weighted path length.
2. **Why use a Min-Heap instead of a sorted array?**
   Extracting the two minimums and inserting their sum takes $O(\log k)$ in a binary min-heap ($O(k \log k)$ total). A sorted array would require $O(k^2)$ time.
3. **What is a Prefix-Free code?**
   No assigned code word is a prefix of any other code word, enabling continuous bitstream decoding without delimiters.
4. **What is Canonical Huffman?**
   Only stores code lengths in the header ($\le 2$ bytes/symbol), eliminating the need to transmit the 1,000-byte tree.
5. **Can lossless compression compress every file?**
   No, by the Pigeonhole Principle. Random data expands; our `STORE` fallback limits expansion to 17 bytes.
6. **How is bit-level I/O implemented?**
   `BitWriter` buffers bits into an 8-bit accumulator (`uint8_t cur_`) and flushes full bytes.
7. **How do you guarantee data integrity?**
   CRC-32 checksum in the container header is recomputed and compared upon decompression.

---

## 9. How to Run the Project

```bash
# 1. Compile C++ CLI engine:
g++ -std=c++14 -O2 -Wall -Wextra -Wpedantic -o huffz.exe src/main.cpp

# 2. Run automated test suite (59 checks):
./tests/tests.exe

# 3. Compress a file:
./huffz.exe c samples/source_code.cpp compressed.huff

# 4. Decompress a file:
./huffz.exe d compressed.huff restored.txt

# 5. Verify bit-for-bit equality (Objective 10):
./huffz.exe verify samples/source_code.cpp restored.txt

# 6. Run benchmark across all input types:
./huffz.exe bench samples/*

# 7. Launch Python Tkinter GUI (Objective 8):
python gui.py
```
