# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] — M1 foundation (2026-10-13)

### Added

- Pinned client-mod build: LeviLamina `26.51.*` in `xmake.lua` (default
  `target_type=client`, target `architect`) and `tooth.json` (client-only
  variant); new SDK-free `architect-tests` target.
- SDK-free core under `src/architect/`: canonical schematic model,
  bit-packing helpers, NBT interface, reader contracts, mapping policy,
  placement math, verifier taxonomy, render snapshot, bindings, config.
- Thin `ArchitectMod` lifecycle entry (`src/mod/`); template `MyMod`
  removed.
- Fixture pipeline: `tools/make_litematic_fixture.py` producing
  `fixtures/small.litematic` (3x2x4, 7-state palette, word-straddling
  index 21) + `small.expected.json`.
- Executed Python suites (129 checks): bitpack, transforms,
  fixture conformance, verifier taxonomy, world-to-screen math.
- C++ test mirrors (`tests/cpp/`, proposed — await toolchain).
- Isolated render PoC math (`experiments/render-poc/w2s_math.h`) +
  execution procedure; documented M1 blocker assessment.
- Docs: data model, NBT interface, module boundaries, toolchain
  evidence, render assessment, M1 engineering report; feasibility
  report copied to `docs/reference/`.

### Notes

- NBT backend, format decoders, mapping table, game adapters, and hook
  code are loud stubs or written procedures — see
  `docs/ENGINEERING_REPORT_M1.md` for completed-vs-proposed status.

## [Unreleased] — M2 data pipeline (2026-10-13)

### Added

- Hand-rolled NBT backend (BE/LE, no exceptions, no new dependencies);
  gzip inflate documented as the single remaining step (zlib via xmake).
- Implemented `.litematic` decoder (`readLitematicFromRoot`) + `TileEntity`
  model extension.
- Implemented mapping policy (Exact/Partial/Placeholder, waterlogged
  handling, no invented renames).
- New tests: `TestNbt`/`TestReader`/`TestMapping` (C++, executed:
  79/79 green under MSVC 14.51, `/W4` clean), `test_nbt.py`/`test_mapping.py`
  (Python: 159/159 green overall).
- `.github/workflows/ci.yml`: SDK-free core CI (Windows + Linux),
  fixture reproducibility, Python suites, hygiene guards. Written and
  validated, not yet executed on runners.
- `docs/PLAYBACK_STUDY.md` (camera/matrix/hook findings, AGPL-3.0 —
  patterns only, nothing copied), `docs/ENGINEERING_REPORT_M2.md`,
  `tools/check_style.py`.
- `set_warnings("all")` on the `architect-tests` target (-Werror
  deliberately deferred until all compilers observed green).

## [Unreleased] — M2 follow-up: real file loading (2026-10-13)

### Added

- Dependency-free gzip/DEFLATE inflater (`nbt/Gzip.{h,cpp}`) wired into
  `readLitematic(fileBytes)`; the zlib plan is dropped (nothing to install).
- `TestGzip.cpp` (37 checks, executed): independent zlib-generated vectors
  (`tools/gen_gzip_vectors.py` → `tests/cpp/GzipVectors.h`), corruption
  battery, and end-to-end decode of the real 2-region
  `fixtures/small.litematic` (positions, palettes, states, tile entities).
- Second fixture region ("upper", own palette + furnace tile entity);
  Python conformance extended; `docs/OVERLAY_PLAN.md`;
  `docs/ENGINEERING_REPORT_M2_FOLLOWUP.md`; CI extended (generated-file
  reproducibility, wider hygiene guard).

### Results

- C++ 116/116 green (`/W4` clean); Python suites green; fixture
  generator deterministic. CI ran for real on 2026-10-13: core-ci 4/4
  green (Windows + Linux + fixtures + hygiene) after fixing three
  genuine issues (zlib-coupled byte-diff, top-level requires resolved
  at configure → new `--core_only=y`, xmake flag order + fixture CWD).
  Full mod build (client + server, LeviLamina 26.51.7) green after
  bumping the mod target to C++23. No in-game rendering test yet.

