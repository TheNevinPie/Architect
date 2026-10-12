# Architect — Module Boundaries (M1)

```
file bytes
  └─ schematic/Readers.h      format decoders -> core model (M2; M1 stubs)
       └─ nbt/Nbt.h           vendor-neutral NBT facade (M1: interface + accessors)
  └─ core/Schematic.h         canonical model: Region/Palette/BlockState (M1: DONE)
  └─ mapping/BlockMap.h       Java/Bedrock translation, Exact/Partial/
                              Placeholder/Unsupported (M2; M1 stub=Unsupported)
  └─ transform/Placement.h    anchor + rotation + mirror on POSITIONS only (M1: DONE)
       ══ game boundary (adapters live outside src/architect/, use the SDK) ══
       ├─ BlockPos::transform / VanillaBlockStateTransformUtils for state rotation
       ├─ BlockSource reads on the game thread -> world/Verifier.h (M1: DONE, pure)
       ├─ RenderSnapshot.h published game-thread -> drawn by overlay (M1: types DONE)
       └─ KeyRegistry + chat/HUD wired to ui/Controls.h ids (M2)
  └─ config/ArchitectConfig.h plain-data schema (M1: DONE)
  └─ mod/ArchitectMod.h       thin lifecycle only; no feature logic (M1: DONE)
```

## Rules

| # | Rule |
|---|---|
| 1 | Nothing under `src/architect/` includes a LeviLamina/Minecraft header. Ever. |
| 2 | Game helpers are called only from adapters outside the core. `transform/` moves positions; the adapter rotates states with the game's own utils. |
| 3 | `BlockSource` is touched on the game thread only. The render thread draws exclusively from the last published `RenderSnapshot`. |
| 4 | Unknown blocks are placeholders + warnings, never silent air. |
| 5 | Unloaded chunks classify as `Unchecked`, never `Correct`. |
| 6 | The verifier never writes to the world. Read-only by construction (it receives a `BlockQuery`, not a `BlockSource`). |
| 7 | Rendering experiments live in `experiments/` and are NOT part of the default xmake target until the PoC gate passes. |

## What the M1 core proves without the SDK

`Placement` (rotations/mirrors/anchors), `Bits` (palette unpacking),
`Verifier::diffIncremental` (5-state taxonomy + tick budgets), snapshot
colouring, bindings, and config schema all compile to pure logic with
zero engine dependency. The Python tests execute the same algorithms
today; the C++ mirrors in `tests/cpp/` execute them under xmake once the
toolchain is present.
