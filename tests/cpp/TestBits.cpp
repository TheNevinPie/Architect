// M1 PROPOSED (runs under xmake once the toolchain lands; see docs/TOOLCHAIN.md).
// Mirrors tests/python/test_bitpack.py vectors against architect/core/Bits.h.
// stdlib only: no test framework dependency by design.
#include "architect/core/Bits.h"

#include <cassert>
#include <cstdio>

namespace {

int failures = 0;

void check(const char* name, bool cond) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) {
        ++failures;
    }
}

} // namespace

int testBits() {
    using namespace architect::detail;

    check("bits empty -> nullopt", !bitsPerEntry(0).has_value());
    check("bits 1 -> 2", bitsPerEntry(1) == 2);
    check("bits 4 -> 2", bitsPerEntry(4) == 2);
    check("bits 7 -> 3", bitsPerEntry(7) == 3);
    check("bits 9 -> 4", bitsPerEntry(9) == 4);
    check("len 24 cells @3b = 2 words", packedLongCount(24, 3) == 2);
    check("len 33 cells @2b = 2 words", packedLongCount(33, 2) == 2);

    // Hand-packed fixture words: recompute here with the inverse operation
    // (pack LSB-first) and verify unpackEntry inverts it, incl. straddle.
    const std::vector<std::uint32_t> ids = {1, 0, 3, 0, 2, 0, 0, 0, 0, 6, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 1, 0, 4, 0, 5};
    constexpr int kBits = 3;
    std::vector<std::uint64_t> raw(packedLongCount(ids.size(), kBits), 0);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const std::uint64_t start = i * kBits;
        const std::uint64_t wi    = start / 64ULL;
        const unsigned off        = static_cast<unsigned>(start % 64ULL);
        raw[wi] |= static_cast<std::uint64_t>(ids[i]) << off;
        if (off + kBits > 64) {
            raw[wi + 1] |= static_cast<std::uint64_t>(ids[i]) >> (64 - off);
        }
    }
    std::vector<std::int64_t> words;
    for (auto w : raw) {
        words.push_back(static_cast<std::int64_t>(w));
    }
    check("fixture packs to 2 words", words.size() == 2);
    bool roundTrip = true;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (unpackEntry(words, i, kBits, 7) != ids[i]) {
            roundTrip = false;
            break;
        }
    }
    check("24-cell round-trip incl. straddle idx21", roundTrip);
    check("straddle idx21 == 4", unpackEntry(words, 21, kBits, 7) == 4u);
    check("short array -> nullopt", !unpackEntry({0}, 40, kBits, 7).has_value());

    // Out-of-range id must be rejected, not returned.
    std::vector<std::uint64_t> badRaw(1, 7); // id 7 with palette 7
    std::vector<std::int64_t> bad(badRaw.begin(), badRaw.end());
    check("id >= palette -> nullopt", !unpackEntry(bad, 0, kBits, 7).has_value());
    return failures;
}
