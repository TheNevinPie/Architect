# Architect

Litematica-inspired schematic blueprint mod for Minecraft Bedrock
(Windows), built as a native LeviLamina **client** mod (C++20).

Scope (initial): load Java `.litematic` schematics → canonical model →
anchor/rotate in the world → aligned 3D blueprint overlay → world
comparison (missing / correct / incorrect / extra / unchecked). Auto-paste,
settings UI, and native ghost-block rendering are deferred.

## Project state: M1 foundation (2026-10-13)

- Pinned build: LeviLamina `26.51.*`, client-only (`xmake.lua` +
  `tooth.json` agree; the `26.20.*` mismatch from the old workspace was
  not carried over). See `docs/TOOLCHAIN.md`.
- SDK-free core under `src/architect/`: data model, NBT interface,
  placement math, verifier taxonomy, render snapshot, bindings, config.
- Fixture tests: `python tests/python/test_bitpack.py`,
  `test_transforms.py`, `test_litematic_fixture.py`, `test_verifier.py`,
  `test_w2s.py` — all runnable today (stdlib only).
- Render PoC: projection math + written procedure; hook execution is
  blocked on toolchain/game (documented in `docs/RENDER_POC.md`).

## Layout

```
src/architect/   SDK-free core (core, nbt, schematic, mapping,
                 transform, world, render, ui, config)
src/mod/         thin LeviLamina entry point (ArchitectMod)
tests/python/    COMPLETED suites (run now) + algorithm mirrors
tests/cpp/       PROPOSED C++ mirrors (run under xmake when toolchain lands)
fixtures/        small.litematic + small.expected.json (generator: tools/)
experiments/     isolated render PoC (never in the default build)
docs/            model / interfaces / boundaries / toolchain / render / reports
```

## Build (requires toolchain — see docs/TOOLCHAIN.md)

```powershell
xmake f -p windows -a x64 -m release --target_type=client -y
xmake -y                                   # builds `architect` (+ tests)
xmake build architect-tests; xmake run architect-tests
```

## Docs

- `docs/ENGINEERING_REPORT_M1.md` — milestone report + go/no-go.
- `docs/reference/ARCHITECT_FEASIBILITY_REPORT.md` — prior audit (copy;
  original untouched in the old workspace).
- `docs/DATA_MODEL.md`, `docs/NBT_INTERFACE.md`,
  `docs/MODULE_BOUNDARIES.md`, `docs/TOOLCHAIN.md`, `docs/RENDER_POC.md`.

## Rules for contributors

No speculative engine APIs. No unnecessary dependencies. Nothing outside
this directory. Experiments stay isolated. Unchecked ≠ correct. The
verifier never writes to the world.
