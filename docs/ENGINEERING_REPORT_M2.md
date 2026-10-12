# Architect — Engineering Report M2: Data Pipeline (2026-10-13)

Follows `docs/ENGINEERING_REPORT_M1.md`. All changes uncommitted, inside
`C:\Projects\Architect` only.

## Delivered

- **NBT backend: implemented, no vendor library.** `Nbt.cpp` now parses
  all 12 tag types, big- + little-endian, with a bounded cursor
  (truncation/over-long arrays/deep nesting → `ParseError`, never
  over-reads), trailing-byte rejection, and zero exceptions. The M1
  mcnbt/nbt-cpp plan is superseded (recorded in `docs/NBT_INTERFACE.md`):
  the needed subset is small and tested; only **gzip inflate** still
  needs zlib via xmake. `.mcstructure` (raw LE) already parses
  end-to-end; `.litematic` file bytes fail loudly naming the zlib step.
- **Litematica decoder: implemented** (`readLitematicFromRoot`):
  Version 2–7 gate, signed-Size `abs()`, palette decode with string
  properties, `BlockStates` length + id-range validation, tile entities
  (id + cell; malformed entries never fail the blocks). Model extended
  with `TileEntity` (`docs/DATA_MODEL.md`).
- **Mapping policy: implemented** (`BlockMap.cpp`): air→Exact,
  same-namespace passthrough, per-block state allowlists
  (stairs/chest/log/wall_torch), `waterlogged` never silently dropped
  (Partial + second-layer note), non-vanilla/empty→Placeholder (never
  air). No renames curated — inventing any would violate no-speculation.
- **Executed C++ suites: 79/79 green.** Compiled with local MSVC 14.51
  (`cl /std:c++20 /EHsc /W4`), exit code 0, zero warnings. Two real bugs
  found by the compiler + one by a test, all fixed (optional-unwrap
  discipline in decoder/tests; bad-id test value exceeding bit width).
- **Python suites extended: 159/159 green** (`test_nbt`, `test_mapping`
  added; `test_litematic_fixture` is now the conformance twin of
  `TestReader`).
- **CI workflow created** (`.github/workflows/ci.yml`, see §CI below).
- **Playback study** (`docs/PLAYBACK_STUDY.md`): camera/matrix/hook
  findings on the same 26.51 line; AGPL-3.0 → patterns only, nothing copied.

## CI status (honest)

`ci.yml` (jobs: core-windows, core-linux, fixtures, hygiene) is written,
YAML-parsed, action pins verified against the setup-xmake `action.yml`
(`xmake-version: ^3.0.0`), target names taken from `xmake.lua` — but it
has **NOT run on GitHub** (no push authorized). It is a validated file,
not proof. Local equivalents all pass: MSVC C++ suites, Python suites,
fixture regen clean, style clean. `git diff --exit-code` on fixtures and
the ignore-guard directly encode M1 lessons (generator drift, `*.md`
incident).

## Verified vs assumed (updates)

| Item | Status |
|---|---|
| LSB-first packing, index orders, tag names | Verified (litemapy source + executed decoders) |
| yaw/pitch convention, matrix-consume pattern, swapchain≠scene | Verified via Playback source (same 26.51 line) |
| NBT parser correctness (BE/LE/edges) | Executed (TestNbt + test_nbt) |
| Decoder rejections | Executed on both sides (TestReader + fixture suite) |
| gzip inflate via zlib | Assumed path, NOT executed (needs xmake) |
| Full `architect` mod build | NOT attempted (needs xmake + LeviLamina package) |
| In-game render alignment | NOT attempted (needs game; Playback raises confidence only) |

## Remaining blockers

1. xmake + Clang + LeviLamina headers (CI execution, zlib inflate, mod build).
2. GDK game + launcher (render PoC run).
3. `.mcstructure` axis order, `.schem` decoder, full mapping table (scoped M3+).

## Go / no-go: GO for M3 (adapters + PoC execution prep)

Data pipeline is done and proven without the SDK. Next: game-facing
adapters (BlockSource verifier, setupCamera overlay PoC) as soon as the
toolchain + game exist; CI first run immediately after the next
authorized push.
