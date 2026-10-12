"""M2 COMPLETED: nbt_min writer/reader round-trip + corruption behavior.

nbt_min is test support (not shipped), but the M2 C++ backend must match
it: this suite pins the byte-level contract both implement.

Run: python tests/python/test_nbt.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from nbt_min import (  # noqa: E402
    TAG_BYTE, TAG_BYTE_ARRAY, TAG_COMPOUND, TAG_INT, TAG_INT_ARRAY,
    TAG_LIST, TAG_LONG, TAG_LONG_ARRAY, TAG_SHORT, TAG_STRING,
    NbtError, c_get, read_root_gzip, write_root_gzip,
)

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


root = {
    "B": (TAG_BYTE, 0x7F - 256),  # signed byte -129? no: use -5 below
    "S": (TAG_SHORT, -300),
    "I": (TAG_INT, 0x01020304),
    "L": (TAG_LONG, -1),
    "T": (TAG_STRING, "héllo"),
    "BA": (TAG_BYTE_ARRAY, [-128, 0, 127]),
    "IA": (TAG_INT_ARRAY, [1, -2, 3]),
    "LA": (TAG_LONG_ARRAY, [-1, 2]),
    "Ls": (TAG_LIST, (TAG_INT, [1, 2])),
    "Sub": (TAG_COMPOUND, {"N": (TAG_STRING, "nested")}),
}
root["B"] = (TAG_BYTE, -5)

data = write_root_gzip(root, "R")
name, back = read_root_gzip(data)
check("root name", name == "R")
check("byte", c_get(back, "B", TAG_BYTE) == -5)
check("short", c_get(back, "S", TAG_SHORT) == -300)
check("int", c_get(back, "I", TAG_INT) == 0x01020304)
check("long", c_get(back, "L", TAG_LONG) == -1)
check("utf8 string", c_get(back, "T", TAG_STRING) == "héllo")
check("bytearray", c_get(back, "BA", TAG_BYTE_ARRAY) == [-128, 0, 127])
check("intarray", c_get(back, "IA", TAG_INT_ARRAY) == [1, -2, 3])
check("longarray", c_get(back, "LA", TAG_LONG_ARRAY) == [-1, 2])
check("list", c_get(back, "Ls", TAG_LIST) == (TAG_INT, [1, 2]))
check("nested", c_get(c_get(back, "Sub", TAG_COMPOUND), "N", TAG_STRING) == "nested")

# corruption is loud (note: gzip.decompress strips bytes after the stream,
# so file-level trailing garbage is unobservable here; the NBT-payload
# trailing check lives in the C++ TestNbt suite instead).
for label, bad in (
    ("truncated", data[:20]),
    ("not-gzip", b"\x00\x01\x02\x03"),
):
    try:
        read_root_gzip(bad)
        check(label + " raises", False)
    except Exception:
        check(label + " raises", True)

try:
    c_get(back, "Missing", TAG_INT)
    check("missing raises", False)
except NbtError:
    check("missing raises", True)
try:
    c_get(back, "T", TAG_INT)
    check("mistyped raises", False)
except NbtError:
    check("mistyped raises", True)

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
