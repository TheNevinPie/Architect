// Gzip + raw DEFLATE decompressor (RFC 1952 + RFC 1951).
// Dependency-free, no-throw, bounded output. Canonical Huffman decoding
// via explicit binary trees; bit order is LSB-first inside each byte.
#include "architect/nbt/Gzip.h"

#include <array>
#include <vector>

namespace architect::gzip {
namespace {

// CRC-32 (IEEE 802.3) over the decompressed payload, verified against
// the gzip trailer.
std::uint32_t crc32(const std::vector<std::uint8_t>& data) {
    static std::uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        ready = true;
    }
    std::uint32_t crc = 0xFFFFFFFFU;
    for (auto b : data) {
        crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) : mPtr(data), mEnd(data + size) {}

    // Read n bits (n <= 16), LSB-first. ok() goes false past the end.
    std::uint32_t bits(int n) {
        while (mHeld < n) {
            if (mPtr >= mEnd) {
                mOk = false;
                return 0;
            }
            mBuf |= static_cast<std::uint32_t>(*mPtr++) << mHeld;
            mHeld += 8;
        }
        const std::uint32_t v = mBuf & (n == 32 ? 0xFFFFFFFFU : ((1U << n) - 1U));
        mBuf >>= n;
        mHeld -= n;
        return v;
    }

    void alignToByte() {
        const int skip = mHeld % 8;
        mBuf >>= skip;
        mHeld -= skip;
    }

    [[nodiscard]] bool ok() const { return mOk; }
    const std::uint8_t* bytePtr() const { return mPtr; }
    std::size_t bytesLeft(const std::uint8_t* end) const {
        return mPtr > end ? 0 : static_cast<std::size_t>(end - mPtr);
    }

private:
    const std::uint8_t* mPtr;
    const std::uint8_t* mEnd;
    std::uint32_t mBuf = 0;
    int mHeld = 0;
    bool mOk = true;
};

// Canonical Huffman tree as an explicit binary tree.
struct HuffmanTree {
    // child[node][0|1], symbols at leaves; -1 = absent.
    std::vector<std::array<int, 2>> child{{{-1, -1}}};
    std::vector<int> symbol{0};

    bool build(const std::vector<int>& lengths, DecompressError& err) {
        // Count codes per length, then assign canonical codes (RFC 1951 §3.2.2).
        int maxLen = 0;
        for (int len : lengths) {
            if (len > maxLen) {
                maxLen = len;
            }
        }
        if (maxLen == 0 || maxLen > 15) {
            err = DecompressError{"bad Huffman code lengths"};
            return false;
        }
        std::vector<int> blCount(maxLen + 1, 0);
        for (int len : lengths) {
            if (len > 0) {
                ++blCount[len];
            }
        }
        std::vector<int> nextCode(maxLen + 1, 0);
        int code = 0;
        for (int bits = 1; bits <= maxLen; ++bits) {
            code = (code + blCount[bits - 1]) << 1;
            nextCode[bits] = code;
        }
        for (std::size_t n = 0; n < lengths.size(); ++n) {
            const int len = lengths[n];
            if (len == 0) {
                continue;
            }
            int c = nextCode[len]++;
            if (c >= (1 << len)) {
                err = DecompressError{"over-subscribed Huffman code"};
                return false;
            }
            // Insert code of `len` bits, MSB-first along the tree.
            int node = 0;
            for (int b = len - 1; b >= 0; --b) {
                const int bit = (c >> b) & 1;
                if (child[node][bit] == -1) {
                    child[node][bit] = static_cast<int>(child.size());
                    child.push_back({{-1, -1}});
                    symbol.push_back(0);
                }
                node = child[node][bit];
            }
            symbol[node] = static_cast<int>(n);
        }
        return true;
    }

    // Returns -1 on invalid encoding or truncated input.
    int decode(BitReader& br) const {
        int node = 0;
        for (;;) {
            if (!br.ok()) {
                return -1;
            }
            const int bit = static_cast<int>(br.bits(1));
            if (!br.ok()) {
                return -1;
            }
            node = child[node][bit];
            if (node < 0) {
                return -1;
            }
            if (child[node][0] == -1 && child[node][1] == -1) {
                return symbol[node];
            }
        }
    }
};

struct LengthDef {
    int base;
    int extra;
};

LengthDef lengthFor(int symbol) {
    static const LengthDef table[] = {
        {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}, {10, 0}, // 257-264
        {11, 1}, {13, 1}, {15, 1}, {17, 1}, // 265-268
        {19, 2}, {23, 2}, {27, 2}, {31, 2}, // 269-272
        {35, 3}, {43, 3}, {51, 3}, {59, 3}, // 273-276
        {67, 4}, {83, 4}, {99, 4}, {115, 4}, // 277-280
        {131, 5}, {163, 5}, {195, 5}, {227, 5}, // 281-284
        {258, 0}, // 285
    };
    if (symbol < 257 || symbol > 285) {
        return {0, -1};
    }
    return table[symbol - 257];
}

LengthDef distFor(int symbol) {
    static const LengthDef table[] = {
        {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 1}, {7, 1}, {9, 2}, {13, 2}, // 0-7
        {17, 3}, {25, 3}, {33, 4}, {49, 4}, {65, 5}, {97, 5}, {129, 6}, {193, 6}, // 8-15
        {257, 7}, {385, 7}, {513, 8}, {769, 8}, // 16-19
        {1025, 9}, {1537, 9}, {2049, 10}, {3073, 10}, // 20-23
        {4097, 11}, {6145, 11}, {8193, 12}, {12289, 12}, // 24-27
        {16385, 13}, {24577, 13}, // 28-29
    };
    if (symbol < 0 || symbol > 29) {
        return {0, -1};
    }
    return table[symbol];
}

bool fixedTables(HuffmanTree& lit, HuffmanTree& dist, DecompressError& err) {
    std::vector<int> litLen(288, 0);
    for (int i = 0; i <= 143; ++i) {
        litLen[i] = 8;
    }
    for (int i = 144; i <= 255; ++i) {
        litLen[i] = 9;
    }
    for (int i = 256; i <= 279; ++i) {
        litLen[i] = 7;
    }
    for (int i = 280; i <= 287; ++i) {
        litLen[i] = 8;
    }
    std::vector<int> distLen(30, 5);
    return lit.build(litLen, err) && dist.build(distLen, err);
}

bool dynamicTables(BitReader& br, HuffmanTree& lit, HuffmanTree& dist, DecompressError& err) {
    const int hlit = static_cast<int>(br.bits(5)) + 257;
    const int hdist = static_cast<int>(br.bits(5)) + 1;
    const int hclen = static_cast<int>(br.bits(4)) + 4;
    if (!br.ok() || hlit > 286 || hdist > 30) {
        err = DecompressError{"bad dynamic block header"};
        return false;
    }
    static const int kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    std::vector<int> clLens(19, 0);
    for (int i = 0; i < hclen; ++i) {
        clLens[kOrder[i]] = static_cast<int>(br.bits(3));
    }
    if (!br.ok()) {
        err = DecompressError{"truncated code-length alphabet"};
        return false;
    }
    HuffmanTree clTree;
    if (!clTree.build(clLens, err)) {
        return false;
    }
    std::vector<int> all(hlit + hdist, 0);
    std::size_t i = 0;
    int prev = 0;
    while (i < all.size()) {
        const int sym = clTree.decode(br);
        if (sym < 0) {
            err = DecompressError{"bad code-length symbol"};
            return false;
        }
        if (sym <= 15) {
            all[i++] = sym;
            prev     = sym;
        } else if (sym == 16) {
            if (i == 0) {
                err = DecompressError{"repeat-16 with no previous length"};
                return false;
            }
            const int rep = static_cast<int>(br.bits(2)) + 3;
            if (!br.ok() || i + rep > all.size()) {
                err = DecompressError{"bad repeat-16 run"};
                return false;
            }
            for (int k = 0; k < rep; ++k) {
                all[i++] = prev;
            }
        } else {
            const int rep = (sym == 17) ? static_cast<int>(br.bits(3)) + 3 : static_cast<int>(br.bits(7)) + 11;
            if (!br.ok() || i + rep > all.size()) {
                err = DecompressError{"bad zero repeat run"};
                return false;
            }
            for (int k = 0; k < rep; ++k) {
                all[i++] = 0;
            }
            prev = 0;
        }
    }
    std::vector<int> litLen(all.begin(), all.begin() + hlit);
    std::vector<int> distLen(all.begin() + hlit, all.end());
    if (!lit.build(litLen, err)) {
        return false;
    }
    // An all-zero distance alphabet means "no distance codes": legal when
    // the block holds no matches (enforced at match time below).
    bool distEmpty = true;
    for (int len : distLen) {
        if (len != 0) {
            distEmpty = false;
        }
    }
    if (distEmpty) {
        return true;
    }
    return dist.build(distLen, err);
}

bool inflateBlock(
    BitReader&         br,
    const HuffmanTree& lit,
    const HuffmanTree& dist,
    bool               distValid,
    std::vector<std::uint8_t>& out,
    std::size_t maxOut,
    DecompressError& err
) {
    for (;;) {
        const int sym = lit.decode(br);
        if (sym < 0) {
            err = DecompressError{"bad literal/length symbol"};
            return false;
        }
        if (sym < 256) {
            if (out.size() >= maxOut) {
                err = DecompressError{"decompressed output exceeds size cap"};
                return false;
            }
            out.push_back(static_cast<std::uint8_t>(sym));
        } else if (sym == 256) {
            return true;
        } else {
            const LengthDef ld = lengthFor(sym);
            if (ld.extra < 0) {
                err = DecompressError{"bad length symbol"};
                return false;
            }
            int length = ld.base + (ld.extra ? static_cast<int>(br.bits(ld.extra)) : 0);
            if (!br.ok()) {
                err = DecompressError{"truncated length extra bits"};
                return false;
            }
            if (!distValid) {
                err = DecompressError{"match with no distance codes"};
                return false;
            }
            const int dsym = dist.decode(br);
            if (dsym < 0) {
                err = DecompressError{"bad distance symbol"};
                return false;
            }
            const LengthDef dd = distFor(dsym);
            if (dd.extra < 0) {
                err = DecompressError{"bad distance code"};
                return false;
            }
            int distance = dd.base + (dd.extra ? static_cast<int>(br.bits(dd.extra)) : 0);
            if (!br.ok()) {
                err = DecompressError{"truncated distance extra bits"};
                return false;
            }
            if (distance <= 0 || static_cast<std::size_t>(distance) > out.size()) {
                err = DecompressError{"match distance beyond output"};
                return false;
            }
            if (out.size() + static_cast<std::size_t>(length) > maxOut) {
                err = DecompressError{"decompressed output exceeds size cap"};
                return false;
            }
            const std::size_t from = out.size() - static_cast<std::size_t>(distance);
            for (int k = 0; k < length; ++k) {
                out.push_back(out[from + k]); // overlap-safe: index re-read each byte
            }
        }
    }
}

} // namespace

DecompressResult decompressGzip(const std::uint8_t* data, std::size_t size, std::size_t maxOutputBytes) {
    if (data == nullptr || size < 10) {
        return DecompressError{"input too short for a gzip header"};
    }
    if (data[0] != 0x1F || data[1] != 0x8B) {
        return DecompressError{"bad gzip magic"};
    }
    if (data[2] != 8) {
        return DecompressError{"unsupported gzip compression method (need deflate)"};
    }
    const std::uint8_t flags = data[3];
    if (flags & 0xE0) {
        return DecompressError{"reserved gzip flag bits set"};
    }
    std::size_t pos = 10; // magic(2) + method + flags + mtime(4) + xfl + os
    if (flags & 0x04) { // FEXTRA
        if (pos + 2 > size) {
            return DecompressError{"truncated gzip extra field"};
        }
        const std::size_t xlen = data[pos] | (static_cast<std::size_t>(data[pos + 1]) << 8);
        pos += 2;
        if (pos + xlen > size) {
            return DecompressError{"truncated gzip extra field"};
        }
        pos += xlen;
    }
    if (flags & 0x08) { // FNAME
        while (pos < size && data[pos] != 0) {
            ++pos;
        }
        if (pos >= size) {
            return DecompressError{"truncated gzip file name"};
        }
        ++pos;
    }
    if (flags & 0x10) { // FCOMMENT
        while (pos < size && data[pos] != 0) {
            ++pos;
        }
        if (pos >= size) {
            return DecompressError{"truncated gzip comment"};
        }
        ++pos;
    }
    if (flags & 0x02) { // FHCRC
        if (pos + 2 > size) {
            return DecompressError{"truncated gzip header CRC"};
        }
        pos += 2;
    }

    BitReader br(data + pos, size - pos);
    std::vector<std::uint8_t> out;
    bool done = false;
    while (!done) {
        const int bfinal = static_cast<int>(br.bits(1));
        const int btype = static_cast<int>(br.bits(2));
        if (!br.ok()) {
            return DecompressError{"truncated deflate block header"};
        }
        done = (bfinal != 0);
        if (btype == 0) {
            br.alignToByte();
            const std::uint8_t* p = br.bytePtr();
            const std::uint8_t* end = data + size;
            if (br.bytesLeft(end) < 4) {
                return DecompressError{"truncated stored block lengths"};
            }
            const unsigned len = p[0] | (static_cast<unsigned>(p[1]) << 8);
            const unsigned nlen = p[2] | (static_cast<unsigned>(p[3]) << 8);
            if ((len ^ nlen) != 0xFFFFU) {
                return DecompressError{"stored block LEN/NLEN mismatch"};
            }
            if (br.bytesLeft(end) < 4 + len) {
                return DecompressError{"truncated stored block data"};
            }
            if (out.size() + len > maxOutputBytes) {
                return DecompressError{"decompressed output exceeds size cap"};
            }
            out.insert(out.end(), p + 4, p + 4 + len);
            // Rebuild the reader past the consumed bytes.
            const std::size_t consumed = static_cast<std::size_t>((p + 4 + len) - (data + pos));
            br = BitReader(data + pos + consumed, size - pos - consumed);
        } else if (btype == 1 || btype == 2) {
            HuffmanTree lit, dist;
            DecompressError treeErr;
            if (btype == 1) {
                if (!fixedTables(lit, dist, treeErr)) {
                    return treeErr;
                }
            } else if (!dynamicTables(br, lit, dist, treeErr)) {
                return treeErr;
            }
            // A default-constructed tree (single childless root) means the
            // dynamic header carried no distance codes: matches are illegal.
            const bool distValid =
                !(dist.child.size() == 1 && dist.child[0][0] == -1 && dist.child[0][1] == -1);
            if (!inflateBlock(br, lit, dist, distValid, out, maxOutputBytes, treeErr)) {
                return treeErr;
            }
        } else {
            return DecompressError{"reserved deflate block type"};
        }
    }
    if (!br.ok()) {
        return DecompressError{"truncated deflate stream"};
    }
    br.alignToByte();
    const std::uint8_t* p = br.bytePtr();
    const std::uint8_t* end = data + size;
    if (br.bytesLeft(end) < 8) {
        return DecompressError{"truncated gzip trailer"};
    }
    const std::uint32_t wantCrc =
        static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    const std::uint32_t wantSize =
        static_cast<std::uint32_t>(p[4]) | (static_cast<std::uint32_t>(p[5]) << 8) |
        (static_cast<std::uint32_t>(p[6]) << 16) | (static_cast<std::uint32_t>(p[7]) << 24);
    if (crc32(out) != wantCrc) {
        return DecompressError{"gzip CRC32 mismatch (corrupt payload)"};
    }
    if ((out.size() & 0xFFFFFFFFULL) != wantSize) {
        return DecompressError{"gzip ISIZE mismatch (corrupt payload)"};
    }
    if (br.bytesLeft(end) != 8) {
        return DecompressError{"trailing bytes after gzip member"};
    }
    return out;
}

} // namespace architect::gzip
