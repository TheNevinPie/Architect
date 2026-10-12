# Architect — Engineering Report M2 follow-up: real file loading (2026-10-13)

Follows `docs/ENGINEERING_REPORT_M2.md`. Uncommitted, Architect root only.

## P1 — real `.litematic` file loading: COMPLETE

- **Gzip implemented, zero dependencies** (`nbt/Gzip.{h,cpp}`):
  RFC 1952 container (magic, method, FEXTRA/FNAME/FCOMMENT/FHCRC skips)
  + RFC 1951 DEFLATE (stored, fixed-Huffman, dynamic-Huffman with
  repeat-16/17/18, canonical codes, overlap-safe LZ77, distance bounds).
  Trailer CRC32 + ISIZE verified; output capped (default 256 MB);
  trailing members rejected. The M1/M2 zlib plan is officially dead —
  nothing to install, ever.
- **End-to-end C++ test passes** (`TestGzip.cpp`, 37 checks): vectors
  generated from CPython's zlib (`tools/gen_gzip_vectors.py` →
  `tests/cpp/GzipVectors.h`): empty/stored/fixed/dynamic/named/8 KB
  random/100 KB text/2 MB-zeros members; corrupt magic, truncation, cut
  trailer, CRC flip, ISIZE flip, 1 KB cap on the bomb, concatenated
  member, valid-gzip-invalid-NBT — then the real file:
  `fixtures/small.litematic` (628 bytes) → inflate → parse → decode →
  2 regions, 28 cells, both palettes with states, both tile entities.
- **Fixture grown to 2 regions** (3×2×4 + 2×1×2, independent palettes,
  positions, tile entities); Python conformance extended in step.
- **Results:** C++ 116/116 pass (`cl /std:c++20 /EHsc /W4`, exit 0,
  zero warnings); Python 159+... (rerun below) green. Ordinary
  `.litematic` loading may now be reported complete **for the
  gzip→model path** (uncompressed-LE `.mcstructure` parses; `.schem`
  still deferred; negative sizes covered in unit tests).

## P2 — GitHub Actions: RAN, ALL GREEN (2026-10-13)

With push authorization, `core-ci` executed for real — and the first
run caught **three genuine issues, all fixed without suppression**:

1. **Fixture byte-diff was environment-coupled.** CI's classic zlib
   emits a different gzip OS byte (and different deflate bytes) than
   local zlib-ng — equally valid members, but `git diff` failed.
   Fix: byte-diff only the zlib-independent `small.expected.json`;
   regenerated binaries are validated by the Python suites instead.
2. **`xmake f` resolves ALL top-level requires.** Core configure tried
   to build the whole LeviLamina tree (Windows-only `symbolprovider`
   broke Linux; the `levilamina` package itself failed on Windows).
   Fix: `--core_only=y` option skips repos/requires/mod target —
   core CI is now fully hermetic (no downloads).
3. **xmake CLI flag order + fixture CWD.** Flags must precede the target
   name; `xmake run` executes inside `build/`, so the E2E leg reads
   `$ARCHITECT_FIXTURES_DIR` (exported in CI, probed locally).
4. **Mod target needs C++23.** Full-mod build failed in
   `ll/api/reflection/Reflection.h`: `std::to_underlying` under the
   template's `c++20`. Bumped the `architect` target only; core stays C++20.

Final state (2026-10-13):
**core-ci 4/4 green** (Windows + Linux C++ suites incl. file end-to-end,
fixtures + Python, hygiene) and **build.yml client + server green**
(the complete native mod compiles against LeviLamina 26.51.7).
Fix arc across runs `38299325211` → `38300807418` → `38301224888` →
`38302235941` (first all-green core-ci) → `38304818123` + `38304818120`
(final tree, both workflows green).
Workflow: https://github.com/TheNevinPie/Architect/actions
(select the `core-ci` workflow on the left; per-job logs and the
"0 failure(s)" lines are in the run pages).

## P3 — rendering experiment: DESIGNED, NOT RUN

`docs/OVERLAY_PLAN.md` records the camera-matrix + screen-space-box
approach (frame-tagged `bgfx` submission preferred, `setupCamera`
second, angles never), stage-A cube gate, threading/resize/cleanup
rules. No hook code written, no Playback code copied, no Playback
dependency, no game run. Parser and renderer stay separated by the
`RenderSnapshot` boundary.

## Remaining blockers

1. ~~Push authorization → CI first run → xmake/clang toolchain → full mod build~~
   DONE 2026-10-13 (CI green, mod builds; local xmake/Clang still absent).
2. GDK game + launcher → in-game cube gate (the last unverified claim).
3. `.schem` decoder, `.mcstructure` axis question, full mapping table.
