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

## P2 — GitHub Actions: READY, NOT RUN

`ci.yml` extended (generated-vectors reproducibility, wider hygiene
guard incl. the new files) and re-validated (YAML parse, pins,
targets). Execution needs a push, which is **not authorized**
(uncommitted-changes + no-push instructions stand). Exact state and how
to run it are in the final report. No job has run anywhere; nothing is
claimed.

## P3 — rendering experiment: DESIGNED, NOT RUN

`docs/OVERLAY_PLAN.md` records the camera-matrix + screen-space-box
approach (frame-tagged `bgfx` submission preferred, `setupCamera`
second, angles never), stage-A cube gate, threading/resize/cleanup
rules. No hook code written, no Playback code copied, no Playback
dependency, no game run. Parser and renderer stay separated by the
`RenderSnapshot` boundary.

## Remaining blockers (unchanged in kind)

1. Push authorization → CI first run → xmake/clang toolchain → full mod build.
2. GDK game + launcher → in-game cube gate.
3. `.schem` decoder, `.mcstructure` axis question, full mapping table.
