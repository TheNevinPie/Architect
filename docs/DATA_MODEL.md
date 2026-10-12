# Architect — Schematic Data Model (M1)

Source of truth: `src/architect/core/Schematic.h` (+ `Bits.h`, `Vec3i.h`).
This document explains the model; it does not restate it field-by-field.

## Design principles

1. **Readers are lossless.** `BlockState` keeps the source-namespace name
   and all properties as strings. Translation to Bedrock happens only in
   the mapping layer (`mapping/BlockMap.h`), never in a reader.
2. **No engine types in the core.** Everything under `src/architect/` is
   plain C++20 + STL. Game helpers (`BlockPos::transform`,
   `VanillaBlockStateTransformUtils`) are called only from future
   game-facing adapters, which are outside the core.
3. **Silent air substitution is forbidden.** Unknown blocks become
   `Placeholder`/`Unsupported` with a counted warning, never air.

## Entities

- `Vec3i` — integer coordinate (region-local cell, size, world anchor).
- `BlockState` — `{name, states: map<string,string>, isAir}`.
- `Region` — `{name, size, position, palette, cells}`. `cells` holds one
  palette id per cell in the file's **native index order**:
  - litematic: `y * (sizeX * sizeZ) + z * sizeX + x` (x fastest)
  - schem v3: `x + z * sizeX + y * sizeX * sizeZ` (x fastest, y slowest)
- `Schematic` — `{metadata, regions[]}` + `enclosedVolume()`.

## Litematica packing (normative for the M2 decoder)

- `BlockStates` is a big-endian long array, `bits = max(2, ceil(log2(|palette|)))`
  bits per entry, x-fastest in the index order above.
- Entries may **straddle** 64-bit boundaries; extraction is LSB-first
  within each 64-bit word — CONFIRMED against `litemapy/storage.py`
  `LitematicaBitArray.__getitem__` (fetched 2026-10-13, branch `master`):
  `array[start] >> start_bit_offset & mask`, straddle ORs the next word
  shifted by `64 - start_bit_offset`. `src/architect/core/Bits.h`
  implements the same algorithm; `tests/python/litematic_bitpack.py`
  mirrors it and the vectors cross-check.
- Region tags (per `litemapy/schematic.py`, branch `master`): `Position`,
  `Size` (signed), `BlockStatePalette` (list of `{Name, Properties?}`),
  `BlockStates` (long[]), `Entities`, `TileEntities`, `PendingBlockTicks`,
  `PendingFluidTicks`. Root tags: `Version`, `SubVersion`,
  `MinecraftDataVersion`, `Metadata{...}`, `Regions{...}`.
- Array length MUST equal `ceil(volume * bits / 64)`; any mismatch is a
  hard `ReadError`.
- `Size` may be signed in-file: normalize with `abs()`. Palette id 0 is
  conventionally air but the palette entry is authoritative, not the id.
- `Version`: accept 2–7 (current 7); reject `1` and `> 7` with clear errors.
- `TileEntity{id, pos}` (M2 addition): region-local tile entities decode
  with id + cell; full per-type NBT comparison is deferred (verifier
  limitation — blocks never fail because of a malformed tile entity).

## Implementation status (M2, 2026-10-13)

`readLitematicFromRoot` in `src/architect/schematic/Readers.cpp` is
IMPLEMENTED and EXECUTED (`tests/cpp/TestReader.cpp`, MSVC 14.51,
green): fixture-shape decode, straddle cell, signed-size abs(), all five
rejections, tile entities. End-to-end file bytes (gzip) await zlib.

## Verification status (honest ledger)

| Claim | Status |
|---|---|
| Index order, bits/entry, array length, signed Size, palette-0-air, region/root tag names | CONFIRMED against `litemapy/schematic.py`+`storage.py` (`master`, fetched 2026-10-13) |
| LSB-first extraction within words | CONFIRMED (same source, `LitematicaBitArray.__getitem__`); mirrored in `Bits.h` + `litematic_bitpack.py` |
| `.schem` v3 index order + varint `Data` | Design-from-evidence (Sponge spec + WorldEdit readers) — decoder not started |
| `.mcstructure` flat-index axis order | OPEN QUESTION — wiki/gist/crate say z-fastest, one editor page implies y-fastest; needs a 2×2×2 empirical file |

The Python fixture tests (`tests/python/`) pin the *implemented* math;
cross-validation against a real Litematica-authored file is M2 work.
