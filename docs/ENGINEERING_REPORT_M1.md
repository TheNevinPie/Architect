# Architect — Engineering Report M1: Foundation (2026-10-13)

## Delivered

- **Self-contained structure.** `src/architect/` (9 SDK-free modules),
  `src/mod/` (thin `ArchitectMod` entry), `tests/{python,cpp}`,
  `fixtures/`, `tools/`, `experiments/render-poc/`, `docs/`. Independent
  repo at `C:\Projects\Architect` (origin `TheNevinPie/Architect`);
  pocket-on-pc untouched (feasibility report COPIED to
  `docs/reference/`, byte-identical, original in place).
- **Reproducible build config.** `target_type` default `client`, target
  `architect`, LeviLamina pinned `26.51.*` in BOTH `xmake.lua` and
  `tooth.json` (client-only variant; server variant dropped — this mod
  is client-side). New `architect-tests` binary target (SDK-free core +
  C++ mirrors, host MSVC). CI matrix retained as reference build.
- **Data model + NBT interface.** `core/Schematic.h` (lossless
  Region/Palette/BlockState, native index orders), `core/Bits.h`
  (LSB-first unpack incl. straddle — confirmed line-for-line against
  `litemapy/storage.py`), `nbt/Nbt.h` (vendor-neutral facade; accessors
  real, `readRootCompound` loud stub), `schematic/Readers.h` (contracts +
  hard-error rules), `mapping/`, `transform/` (positions only),
  `world/Verifier.h` (5-state taxonomy + budgets, pure over a
  `BlockQuery`), `render/RenderSnapshot.h` (thread-handoff types),
  `ui/`, `config/`. Stubs link cleanly and fail loudly; nothing claims
  an unbuilt backend.
- **Parser + transform tests (COMPLETED, executed — 129 checks, 0 failures).**
  - `test_bitpack.py` — 32/32: bits/entry table, word counts, both index
    orders, 300 randomized round-trips, straddle pin (idx 21), rejections.
  - `test_transforms.py` — 23/23: size swaps, hand-derived rotation
    corners, mirrors, Litematica mirror-then-rotate order, 4×CW90 and
    CW90+CCW90 identity over all 24 cells, bounds, anchored toWorld.
  - `test_litematic_fixture.py` — 50/50: real gzip+NBT bytes of a
    3×2×4 7-state fixture decoded by an independent reader; Version
    gating, palette, straddle cell, tile entity, corruption → hard error.
  - `test_verifier.py` — 14/14: hand-computed taxonomy rig, budget
    determinism, correct-cells-not-stored.
  - `test_w2s.py` — 10/10: projection centering/edges/behind-camera/
    resize/strafe/cube-span via independent reimplementation.
- **Render PoC / blocker.** Hook execution is BLOCKED (no xmake/clang/SDK
  headers/game on this machine — itemized evidence in
  `docs/TOOLCHAIN.md`); instead M1 ships tested projection math
  (`w2s_math.h`), a 4-measurement camera-input plan, threading/lifecycle
  contract, gate criteria, and a step-by-step execution procedure
  (`experiments/render-poc/README.md`, `docs/RENDER_POC.md`).

## Proposed (written, not executed)

C++ mirrors in `tests/cpp/` + `architect-tests` target; C++ NBT backend
+ decoders; mapping table; game adapters; hook code. Each is marked
PROPOSED at its location and gated on the toolchain + PoC.

## Unresolved assumptions (ledger)

1. `.mcstructure` flat-index axis order (z- vs y-fastest) — needs 2×2×2 file.
2. NDC z-range + FOV/aspect/yaw-pitch sources — PoC measurements (a)–(d).
3. `renderLevel` signature / `FrameRenderObject` layout / matrix getters —
   reverify against pinned 26.51 headers in PoC.
4. Exact resolved `levilamina`/`bedrockdata` versions — record at first configure.
5. Settings-remap UI, Forms-on-client, toast/actionbar — unchanged from
   feasibility (UNRESOLVED, post-MVP).

## Go / no-go: CONDITIONAL GO

- **GO** for M2 (NBT backend + Litematica decoder + mapping table +
  verifier adapter design): no new engine evidence needed; the fixture
  suite is a ready conformance target and the C++ mirrors await CI.
- **GATED** for any world-space rendering: PoC-1 procedure is written and
  must be executed (toolchain + game required) before schematic-renderer
  work begins. HUD-only guidance remains the fallback if PoC-1 fails.
- What would flip this to NO-GO: toolchain/SDK unavailable indefinitely
  (then: Python-spec + CI-only development), or PoC-1 failing after fair
  execution (then: re-scope to HUD-only per feasibility §11).

No server-placement compatibility is claimed. No engine behavior is
asserted beyond cited sources. Completed vs proposed is distinguished
throughout.
