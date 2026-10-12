"""Python mirror of src/architect/core/Bits.h.

Implements the SAME algorithm as the C++ helpers (LSB-first packing,
straddling entries). Ground truth: litemapy/storage.py
LitematicaBitArray (branch master, fetched 2026-10-13).

Any divergence between this file and Bits.h is a bug in one of them;
tests/python/test_bitpack.py pins shared vectors.
"""

from math import ceil, log2


def bits_per_entry(palette_size: int):
    """max(2, ceil(log2(palette_size))); None for an empty palette."""
    if palette_size == 0:
        return None
    bits = 0
    n = 1
    while n < palette_size:
        n <<= 1
        bits += 1
    return max(bits, 2)


def packed_long_count(volume: int, bits: int) -> int:
    """ceil(volume * bits / 64)."""
    return (volume * bits + 63) // 64


def pack_entries(ids, bits: int):
    """Pack palette ids LSB-first into unsigned 64-bit words (test helper)."""
    words = [0] * packed_long_count(len(ids), bits)
    mask = (1 << 64) - 1
    for index, value in enumerate(ids):
        assert 0 <= value < (1 << bits), (index, value, bits)
        start = index * bits
        wi = start // 64
        off = start % 64
        words[wi] = (words[wi] | ((value << off) & mask)) & mask
        if off + bits > 64:
            words[wi + 1] = (words[wi + 1] | (value >> (64 - off))) & mask
    return words


def to_signed(words):
    """Unsigned 64-bit words -> signed Java longs (as stored in NBT)."""
    return [w - (1 << 64) if w & (1 << 63) else w for w in words]


def unpack_entry(signed_words, index: int, bits: int, palette_size: int):
    """Mirror of architect::detail::unpackEntry. None on short array/bad id."""
    if bits <= 0 or bits > 64:
        return None
    words = [w & ((1 << 64) - 1) for w in signed_words]
    start_bit = index * bits
    word_pos = start_bit // 64
    bit_offset = start_bit % 64
    if word_pos >= len(words):
        return None
    raw = words[word_pos] >> bit_offset
    if 64 - bit_offset < bits:
        if word_pos + 1 >= len(words):
            return None
        raw |= words[word_pos + 1] << (64 - bit_offset)
    mask = (1 << 64) - 1 if bits == 64 else (1 << bits) - 1
    entry_id = raw & mask
    if entry_id >= palette_size:
        return None
    return entry_id


def flat_index_litematic(x: int, y: int, z: int, sx: int, sz: int) -> int:
    """y * (sx * sz) + z * sx + x (x fastest)."""
    return y * (sx * sz) + z * sx + x


def flat_index_schem_v3(x: int, y: int, z: int, sx: int, sz: int) -> int:
    """x + z * sx + y * sx * sz (Sponge v3)."""
    return x + z * sx + y * sx * sz
