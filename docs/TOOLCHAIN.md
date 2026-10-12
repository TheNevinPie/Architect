# Architect — Toolchain & Target Evidence (M1, 2026-10-13)

## Target version (verified, not copied)

| Item | Value | Source |
|---|---|---|
| LeviLamina line | **26.51.x** (latest line on the supported-versions table: server 26.51.1 / client 26.51.1) | `lamina.levimc.org/versions` (fetched 2026-10-13) |
| xmake-repo entries | `26.51.0`–`26.51.7` (HEAD: `26.51.7` / `f195f48…`) | `xmake-repo/.../levilamina/versions/versions.txt` (fetched 2026-10-13) |
| `xmake.lua` pin | `levilamina 26.51.*`, `target_type` default `client` | this repo |
| `tooth.json` pin | `github.com/LiteLDev/LeviLamina#client: 26.51.*`, client-only variant | this repo |
| Previous mismatch | pocket-on-pc `tooth.json` still pins `26.20.*` while CI built 26.51.6 | feasibility §2; **not carried over** — Architect pins 26.51.* from day one |

`26.51.*` floats across 26.51 patches only. The exact resolved version
MUST be recorded here at first successful configure
(`xmake require --info levilamina`): **pending — no successful configure yet (see blockers).**

## Local machine inventory (2026-10-13, `C:\Projects\Architect`)

| Item | Status | Evidence |
|---|---|---|
| MSVC | PRESENT — VS Community 2026 (`18`), MSVC `14.51.36231`, `Hostx64/x64/cl.exe` | dir listing |
| clang-cl (C++ Clang tools for Windows) | MISSING — `VC\Tools\Llvm\x64\bin` contains only `clang-format.exe`/`clang-tidy.exe` | dir listing |
| xmake | MISSING — not on PATH | `where`/`winget list` |
| Standalone LLVM | MISSING | `winget list`, no `C:\Program Files\LLVM` |
| cmake | PRESENT — 4.4.4 (not used by this build) | `cmake --version` |
| Python | PRESENT — 3.14.7 (fixture tests run on this) | `python --version` |
| `lip` binary | MISSING from PATH (`%LOCALAPPDATA%\lip\` cache/temp dirs exist from prior use) | `where.exe lip` |
| LeviLauncher / GDK game install | NOT FOUND (`C:\XboxGames` holds only unrelated titles) | dir listing |
| LeviLamina SDK/headers | NOT PRESENT locally (arrive via `xmake` package fetch at configure time) | — |

## Consequences (all recorded as blockers, not worked around)

1. **No local xmake/clang-ci build yet**, BUT the SDK-free core +
   tests now compile and run with the machine's MSVC 14.51 directly
   (M2, 2026-10-13): `cl /std:c++20 /EHsc /W4` over
   `src/architect/**.cpp` + `tests/cpp/*.cpp` → 79 checks, 0 failures,
   exit 0, zero warnings (procedure: temp-dir `.bat` invoking
   `vcvars64.bat`; objects/EXEs stay outside the repo). This proves the
   core is genuinely SDK-free. xmake + Clang + LeviLamina headers are
   still missing, so the `architect` mod target and CI remain unverified.
2. **No local game to test against** (no GDK install, no launcher). The
   render PoC therefore ships as isolated, build-gated code plus a written
   procedure — see `docs/RENDER_POC.md`. No engine behavior is claimed.
3. **What IS verified today:** all pure-logic core (`transform`, `Bits`,
   `Verifier`, snapshot colours, bindings, NBT parser, litematic
   decoder, mapping) via the Python fixture tests (159 checks) AND the
   compiled C++ suites (79 checks, MSVC 14.51, exit 0, `/W4` clean).
   The xmake `architect-tests` target + CI workflow (`ci.yml`) are
   written and YAML-validated but NOT yet executed on runners.

## Reproducible-build contract

- `xmake f -p windows -a x64 -m release --target_type=client -y && xmake -y`
  (matches CI release workflow).
- `xmake.lua` + `tooth.json` move together on every LeviLamina bump.
- This file records the exact resolved `levilamina` + `bedrockdata`
  versions after each successful configure. First entry: **pending**.
