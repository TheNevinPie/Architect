// M2 (runs under xmake, or win32 cl.exe directly — see below).
// Hand-built NBT buffers -> readRootCompoundFromMemory: big-endian tags,
// little-endian tags, truncation and corruption behavior. stdlib only.
#include "architect/nbt/Nbt.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int failures = 0;

void check(const char* name, bool cond) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) {
        ++failures;
    }
}

void be16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v));
}
void be32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
}
void be64(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
}
void le32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
}
void tagName(std::vector<std::uint8_t>& b, const char* s, bool le) {
    const std::size_t n = std::strlen(s);
    if (le) {
        b.push_back(static_cast<std::uint8_t>(n));
        b.push_back(static_cast<std::uint8_t>(n >> 8));
    } else {
        be16(b, static_cast<std::uint16_t>(n));
    }
    for (std::size_t i = 0; i < n; ++i) {
        b.push_back(static_cast<std::uint8_t>(s[i]));
    }
}

} // namespace

int testNbt() {
    using namespace architect::nbt;

    // Root compound {byte B=0x7F, short S=-300, int I=0x01020304,
    // long L, string T="hi", list Ls=[1,2], longarray LA=[-1,2]} (BE).
    std::vector<std::uint8_t> be;
    be.push_back(10);
    tagName(be, "", false);
    be.push_back(1);
    tagName(be, "B", false);
    be.push_back(0x7F);
    be.push_back(2);
    tagName(be, "S", false);
    be16(be, 0xFED4); // -300
    be.push_back(3);
    tagName(be, "I", false);
    be32(be, 0x01020304);
    be.push_back(4);
    tagName(be, "L", false);
    be64(be, 0x0102030405060708ULL);
    be.push_back(8);
    tagName(be, "T", false);
    be16(be, 2);
    be.push_back('h');
    be.push_back('i');
    be.push_back(9);
    tagName(be, "Ls", false);
    be.push_back(3); // elem Int
    be32(be, 2);     // len
    be32(be, 1);
    be32(be, 2);
    be.push_back(12);
    tagName(be, "LA", false);
    be32(be, 2);
    be64(be, 0xFFFFFFFFFFFFFFFFULL); // -1
    be64(be, 2);
    be.push_back(0); // End

    auto parsed = readRootCompoundFromMemory(be.data(), be.size(), ByteOrder::Big);
    check("BE parses", std::holds_alternative<Compound>(parsed));
    if (std::holds_alternative<Compound>(parsed)) {
        const Compound& root = std::get<Compound>(parsed);
        check("byte", asByte(**find(root, "B")) == std::int8_t{0x7F});
        check("short -300", asShort(**find(root, "S")) == std::int16_t{-300});
        check("int", asInt(**find(root, "I")) == std::int32_t{0x01020304});
        check("long", asLong(**find(root, "L")) == std::int64_t{0x0102030405060708LL});
        check("string", **asString(**find(root, "T")) == std::string("hi"));
        const List& ls = **asList(**find(root, "Ls"));
        check("list len 2", ls.size() == 2);
        check("list[1] == 2", asInt(ls[1]) == 2);
        const std::vector<std::int64_t>& la = **asLongArray(**find(root, "LA"));
        check("longarray [-1,2]", la.size() == 2 && la[0] == -1 && la[1] == 2);
        check("missing -> nullopt", !find(root, "Nope").has_value());
        check("mistyped accessor -> nullopt", !asInt(**find(root, "T")).has_value());
    }

    // Same int in little-endian.
    std::vector<std::uint8_t> le;
    le.push_back(10);
    tagName(le, "", true);
    le.push_back(3);
    tagName(le, "I", true);
    le32(le, 0x01020304);
    le.push_back(0);
    auto leParsed = readRootCompoundFromMemory(le.data(), le.size(), ByteOrder::Little);
    check("LE parses", std::holds_alternative<Compound>(leParsed));
    if (std::holds_alternative<Compound>(leParsed)) {
        check("LE int", asInt(**find(std::get<Compound>(leParsed), "I")) == std::int32_t{0x01020304});
    }
    // BE bytes misread as LE must NOT give the same value (endianness matters).
    auto cross = readRootCompoundFromMemory(be.data(), be.size(), ByteOrder::Little);
    bool crossDiffers = true;
    if (std::holds_alternative<Compound>(cross)) {
        if (const auto t = find(std::get<Compound>(cross), "I")) {
            crossDiffers = (asInt(**t) != std::int32_t{0x01020304});
        }
    }
    check("BE-as-LE differs", crossDiffers);

    // Corruption is a loud error, never a partial tree.
    auto trunc = readRootCompoundFromMemory(be.data(), be.size() - 3, ByteOrder::Big);
    check("truncated -> ParseError", std::holds_alternative<ParseError>(trunc));
    const std::uint8_t junk[] = {0x1F, 0x8B, 0x00};
    auto junkParsed = readRootCompoundFromMemory(junk, sizeof(junk), ByteOrder::Big);
    check("non-NBT -> ParseError", std::holds_alternative<ParseError>(junkParsed));
    auto empty = readRootCompoundFromMemory(nullptr, 0, ByteOrder::Big);
    check("empty -> ParseError", std::holds_alternative<ParseError>(empty));
    std::vector<std::uint8_t> withTrail = be;
    withTrail.push_back(0xFF);
    auto trail = readRootCompoundFromMemory(withTrail.data(), withTrail.size(), ByteOrder::Big);
    check("trailing byte -> ParseError", std::holds_alternative<ParseError>(trail));

    // gzip flag routes through the inflater: truncated magic fails loudly.
    std::vector<std::uint8_t> gz = {0x1F, 0x8B, 0x08, 0x00};
    auto gzParsed = readRootCompound(gz, ByteOrder::Big, true);
    bool gzLoud = false;
    if (const auto* err = std::get_if<ParseError>(&gzParsed)) {
        gzLoud = err->message.find("gzip") != std::string::npos;
    }
    check("short gzip fails as gzip error", gzLoud);
    return failures;
}
