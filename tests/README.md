# Tests

## COMPLETED (run today — stdlib only, no pytest)

From the project root:

```powershell
python tools\make_litematic_fixture.py
python tests\python\test_bitpack.py            # 32 checks
python tests\python\test_transforms.py         # 23 checks
python tests\python\test_litematic_fixture.py  # 62 checks
python tests\python\test_verifier.py           # 14 checks
python tests\python\test_w2s.py                # 10 checks
python tests\python\test_nbt.py                # 15 checks
python tests\python\test_mapping.py            # 15 checks
```

Each exits non-zero on failure. Mirrors live beside them
(`litematic_bitpack.py`, `placement.py`, `nbt_min.py`); the generator is
`tools/make_litematic_fixture.py` and the binary expectation it produces
is `fixtures/small.litematic` + `fixtures/small.expected.json`.

## EXECUTED C++ (M2-follow-up — MSVC 14.51, no xmake needed)

The `tests/cpp/` suites (`TestBits`, `TestPlacement`, `TestVerifier`,
`TestNbt`, `TestReader`, `TestMapping`, `TestGzip`, `Main`) compile with
the local MSVC directly and pass: **116 checks, 0 failures, exit 0,
`/W4` clean**. Objects/EXEs are built outside the repo (see
`docs/TOOLCHAIN.md`). `GzipVectors.h` is generated
(`python tools/gen_gzip_vectors.py`) and checked in for hermetic builds.

```powershell
xmake build architect-tests; xmake run architect-tests   # CI path (not yet run)
```

Status: suites executed via `cl.exe`; the xmake target + GitHub CI
(`.github/workflows/ci.yml`) are written and validated but await a
runner. CI (`.github/workflows/`) remains the reference for the full
mod build.
