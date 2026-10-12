"""Style hygiene for Architect text sources (run locally + in CI).

Checks, for tracked text files under src/, tests/, tools/, experiments/:
  - no trailing whitespace
  - no tab characters
  - no CRLF line endings (repo uses LF; see .gitattributes)
  - file ends with exactly one newline

Binary fixtures (fixtures/*.litematic) are excluded. This is hygiene,
not formatting: clang-format adoption is deferred until the C++ toolchain
lands (see .github/workflows/ci.yml comments).

Exit non-zero listing every violation.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCOPES = ("src", "tests", "tools", "experiments")
TEXT_EXTS = (".h", ".cpp", ".py", ".md", ".lua", ".json", ".yml", ".txt")
SKIP_DIRS = ("__pycache__",)
SKIP_FILES = ("small.expected.json",)  # generated artifact, checked in as-is

violations = []


def check_file(path, rel):
    with open(path, "rb") as f:
        raw = f.read()
    if b"\r\n" in raw:
        violations.append("%s: CRLF line endings" % rel)
    text = raw.decode("utf-8", errors="replace")
    if not text.endswith("\n") or text.endswith("\n\n"):
        violations.append("%s: must end with exactly one newline" % rel)
    lines = text.split("\n")
    for i, line in enumerate(lines, 1):
        if line != line.rstrip(" \t"):
            violations.append("%s:%d: trailing whitespace" % (rel, i))
        if "\t" in line:
            violations.append("%s:%d: tab character" % (rel, i))


count = 0
for scope in SCOPES:
    base = os.path.join(ROOT, scope)
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if not name.endswith(TEXT_EXTS) or name in SKIP_FILES:
                continue
            full = os.path.join(dirpath, name)
            check_file(full, os.path.relpath(full, ROOT))
            count += 1

print("checked %d files, %d violation(s)" % (count, len(violations)))
for v in violations:
    print("VIOLATION " + v)
sys.exit(1 if violations else 0)
