"""Minimal pure-Python NBT reader/writer (big-endian subset + gzip).

Scope: exactly the tags the Architect fixtures need — Byte, Short, Int,
Long, String, ByteArray, List, Compound, IntArray, LongArray. Named root
compound supported. Used by the fixture generator and the conformance
tests; NOT shipped with the mod (the C++ backend is an M2 deliverable).
"""

import gzip
import struct
from io import BytesIO

TAG_END = 0
TAG_BYTE = 1
TAG_SHORT = 2
TAG_INT = 3
TAG_LONG = 4
TAG_FLOAT = 5
TAG_DOUBLE = 6
TAG_BYTE_ARRAY = 7
TAG_STRING = 8
TAG_LIST = 9
TAG_COMPOUND = 10
TAG_INT_ARRAY = 11
TAG_LONG_ARRAY = 12


class NbtError(ValueError):
    pass


# ---------------- writer ----------------

def _w_string(buf: bytearray, s: str):
    data = s.encode("utf-8")
    buf += struct.pack(">H", len(data))
    buf += data


def _w_payload(buf: bytearray, tag_type: int, value):
    if tag_type == TAG_BYTE:
        buf += struct.pack(">b", value)
    elif tag_type == TAG_SHORT:
        buf += struct.pack(">h", value)
    elif tag_type == TAG_INT:
        buf += struct.pack(">i", value)
    elif tag_type == TAG_LONG:
        buf += struct.pack(">q", value)
    elif tag_type == TAG_STRING:
        _w_string(buf, value)
    elif tag_type == TAG_BYTE_ARRAY:
        buf += struct.pack(">i", len(value))
        buf += struct.pack(">%db" % len(value), *value) if value else b""
    elif tag_type == TAG_INT_ARRAY:
        buf += struct.pack(">i", len(value))
        for v in value:
            buf += struct.pack(">i", v)
    elif tag_type == TAG_LONG_ARRAY:
        buf += struct.pack(">i", len(value))
        for v in value:
            buf += struct.pack(">q", v)
    elif tag_type == TAG_LIST:
        elem_type, items = value
        buf += struct.pack(">b", elem_type)
        buf += struct.pack(">i", len(items))
        for item in items:
            _w_payload(buf, elem_type, item)
    elif tag_type == TAG_COMPOUND:
        for name, (t, v) in value.items():
            buf += struct.pack(">b", t)
            _w_string(buf, name)
            _w_payload(buf, t, v)
        buf += struct.pack(">b", TAG_END)
    else:
        raise NbtError("writer: unsupported tag %r" % tag_type)


def write_root_gzip(root: dict, root_name: str = "") -> bytes:
    """root: {name: (tag_type, value)}. Returns gzip bytes (.litematic file).

    Deterministic: MTIME is fixed at 0 so regenerating fixtures yields
    byte-identical files (CI enforces this with git diff --exit-code).
    """
    import gzip as _gzip
    from io import BytesIO as _BytesIO
    buf = bytearray()
    buf += struct.pack(">b", TAG_COMPOUND)
    _w_string(buf, root_name)
    _w_payload(buf, TAG_COMPOUND, root)
    out = _BytesIO()
    with _gzip.GzipFile(filename="", mode="wb", fileobj=out, mtime=0) as gz:
        gz.write(bytes(buf))
    return out.getvalue()


# ---------------- reader ----------------

class _Reader:
    def __init__(self, data: bytes):
        self._s = BytesIO(data)

    def read(self, n: int) -> bytes:
        b = self._s.read(n)
        if len(b) != n:
            raise NbtError("truncated NBT at offset %d" % self._s.tell())
        return b

    def u8(self): return struct.unpack(">B", self.read(1))[0]
    def i8(self): return struct.unpack(">b", self.read(1))[0]
    def i16(self): return struct.unpack(">h", self.read(2))[0]
    def i32(self): return struct.unpack(">i", self.read(4))[0]
    def i64(self): return struct.unpack(">q", self.read(8))[0]

    def string(self):
        n = struct.unpack(">H", self.read(2))[0]
        return self.read(n).decode("utf-8")

    def payload(self, tag_type: int):
        if tag_type == TAG_BYTE: return self.i8()
        if tag_type == TAG_SHORT: return self.i16()
        if tag_type == TAG_INT: return self.i32()
        if tag_type == TAG_LONG: return self.i64()
        if tag_type == TAG_STRING: return self.string()
        if tag_type == TAG_BYTE_ARRAY:
            n = self.i32()
            if n < 0: raise NbtError("negative bytearray len")
            return list(struct.unpack(">%db" % n, self.read(n))) if n else []
        if tag_type == TAG_INT_ARRAY:
            n = self.i32()
            if n < 0: raise NbtError("negative intarray len")
            return [self.i32() for _ in range(n)]
        if tag_type == TAG_LONG_ARRAY:
            n = self.i32()
            if n < 0: raise NbtError("negative longarray len")
            return [self.i64() for _ in range(n)]
        if tag_type == TAG_LIST:
            elem = self.u8()
            n = self.i32()
            if n < 0: raise NbtError("negative list len")
            return (elem, [self.payload(elem) for _ in range(n)])
        if tag_type == TAG_COMPOUND:
            return self.compound()
        raise NbtError("reader: unsupported tag %r" % tag_type)

    def compound(self):
        out = {}
        while True:
            t = self.u8()
            if t == TAG_END:
                return out
            name = self.string()
            out[name] = (t, self.payload(t))


def read_root_gzip(data: bytes):
    """Returns (root_name, compound_dict). Raises NbtError on corruption."""
    raw = gzip.decompress(data)
    r = _Reader(raw)
    t = r.u8()
    if t != TAG_COMPOUND:
        raise NbtError("root is not a compound (tag %r)" % t)
    name = r.string()
    comp = r.payload(TAG_COMPOUND)
    if r._s.read(1):
        raise NbtError("trailing bytes after root compound")
    return name, comp


def c_get(compound: dict, name: str, tag_type: int):
    """Get (type, value); raises NbtError when absent or mistyped."""
    if name not in compound:
        raise NbtError("missing tag %r" % name)
    t, v = compound[name]
    if t != tag_type:
        raise NbtError("tag %r has type %d, expected %d" % (name, t, tag_type))
    return v
