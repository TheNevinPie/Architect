#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace architect::detail {

// Bit-packing math shared by the Litematica reader and the fixture tests.
//
// Litematica stores region cells as an array of signed 64-bit big-endian
// longs ("BlockStates"). Entries are packed back-to-back at
//   bits = max(2, ceil(log2(paletteSize)))
// bits each, x-fastest in index order
//   index = y * (sizeX * sizeZ) + z * sizeX + x,
// and entries may straddle 64-bit word boundaries. Array length must equal
// ceil(volume * bits / 64) words.
//
// The Python mirror lives in tests/python/litematic_bitpack.py and MUST
// implement the same algorithm; cross-check vectors are in
// tests/python/test_bitpack.py.

// Bits per palette entry. nullopt for an empty palette (invalid file).
inline std::optional<int> bitsPerEntry(std::size_t paletteSize) {
    if (paletteSize == 0) {
        return std::nullopt;
    }
    int bits = 0;
    for (std::size_t n = 1; n < paletteSize; n <<= 1) {
        ++bits;
    }
    return bits < 2 ? 2 : bits;
}

// Expected word count for `volume` cells at `bits` bits each.
inline std::uint64_t packedLongCount(std::uint64_t volume, int bits) {
    return (volume * static_cast<std::uint64_t>(bits) + 63ULL) / 64ULL;
}

// Extract the palette id at flat `index` from packed `words`.
// Returns nullopt when the array is short or an id exceeds paletteSize.
inline std::optional<std::uint32_t> unpackEntry(
    const std::vector<std::int64_t>& words,
    std::uint64_t                    index,
    int                              bits,
    std::size_t                      paletteSize
) {
    if (bits <= 0 || bits > 64) {
        return std::nullopt;
    }
    const std::uint64_t startBit = index * static_cast<std::uint64_t>(bits);
    const std::uint64_t wordPos  = startBit / 64ULL;
    const unsigned bitOffset     = static_cast<unsigned>(startBit % 64ULL);
    if (wordPos >= words.size()) {
        return std::nullopt;
    }
    std::uint64_t raw = static_cast<std::uint64_t>(words[wordPos]) >> bitOffset;
    const unsigned bitsInFirstWord = 64U - bitOffset;
    if (bitsInFirstWord < static_cast<unsigned>(bits)) {
        // Entry straddles into the next word.
        if (wordPos + 1 >= words.size()) {
            return std::nullopt;
        }
        const std::uint64_t next = static_cast<std::uint64_t>(words[wordPos + 1]);
        raw |= next << bitsInFirstWord;
    }
    std::uint64_t mask;
    if (bits == 64) {
        mask = ~0ULL;
    } else {
        mask = (1ULL << static_cast<unsigned>(bits)) - 1ULL;
    }
    const auto id = static_cast<std::uint32_t>(raw & mask);
    if (static_cast<std::size_t>(id) >= paletteSize) {
        return std::nullopt;
    }
    return id;
}

} // namespace architect::detail
