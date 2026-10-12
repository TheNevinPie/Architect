// M2-follow-up: gzip round-trips, corruption behavior, and the full
// compressed-file path (file bytes -> inflate -> NBT -> model).
// Vectors in GzipVectors.h come from CPython's zlib (independent impl).
// stdlib only (+ <fstream> for the fixture-file leg).
#include "GzipVectors.h"

#include "architect/nbt/Gzip.h"
#include "architect/schematic/Readers.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(const char* name, bool cond) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) {
        ++failures;
    }
}

using Bytes = std::vector<std::uint8_t>;

Bytes asBytes(const unsigned char* data, unsigned len) {
    return Bytes(data, data + len);
}

// Independent CRC-32 (test-local): pins meta vectors without sharing
// code with the inflater under test.
std::uint32_t testCrc32(const Bytes& data) {
    static std::uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        ready = true;
    }
    std::uint32_t crc = 0xFFFFFFFFU;
    for (auto b : data) {
        crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

bool gunzipOk(const unsigned char* comp, unsigned compLen, Bytes& out, std::size_t cap = 1ULL << 26) {
    const auto r = architect::gzip::decompressGzip(comp, compLen, cap);
    if (const auto* ok = std::get_if<Bytes>(&r)) {
        out = *ok;
        return true;
    }
    return false;
}

bool gunzipFails(const unsigned char* comp, unsigned compLen, std::size_t cap = 1ULL << 26) {
    Bytes out;
    return !gunzipOk(comp, compLen, out, cap);
}

// Build a gzip member around a raw stored block (independent of zlib).
Bytes storedMember(const Bytes& payload) {
    Bytes m = {0x1F, 0x8B, 0x08, 0x00, 0, 0, 0, 0, 0, 0xFF};
    m.push_back(0x01); // BFINAL=1, BTYPE=00
    const unsigned len = static_cast<unsigned>(payload.size());
    m.push_back(static_cast<std::uint8_t>(len));
    m.push_back(static_cast<std::uint8_t>(len >> 8));
    m.push_back(static_cast<std::uint8_t>(~len));
    m.push_back(static_cast<std::uint8_t>(~len >> 8));
    m.insert(m.end(), payload.begin(), payload.end());
    const std::uint32_t crc = testCrc32(payload);
    for (int i = 0; i < 4; ++i) {
        m.push_back(static_cast<std::uint8_t>(crc >> (i * 8)));
    }
    for (int i = 0; i < 4; ++i) {
        m.push_back(static_cast<std::uint8_t>(len >> (i * 8)));
    }
    return m;
}

bool readFile(const std::string& path, Bytes& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string s = ss.str();
    out.assign(s.begin(), s.end());
    return true;
}

} // namespace

int testGzip() {
    using namespace architect;

    // Exact round-trips (dynamic, fixed, named-member, empty).
    struct SmallCase {
        const char* name;
        const unsigned char* comp;
        unsigned compLen;
        const unsigned char* raw;
        unsigned rawLen;
    };
    const SmallCase small[] = {
        {"empty", kGz_empty, kGz_emptyLen, kRaw_empty, kRaw_emptyLen},
        {"hello", kGz_hello, kGz_helloLen, kRaw_hello, kRaw_helloLen},
        {"bytes256", kGz_bytes256, kGz_bytes256Len, kRaw_bytes256, kRaw_bytes256Len},
        {"fixed", kGz_fixed, kGz_fixedLen, kRaw_fixed, kRaw_fixedLen},
        {"named", kGz_named, kGz_namedLen, kRaw_named, kRaw_namedLen},
    };
    for (const auto& c : small) {
        Bytes out;
        check(c.name, gunzipOk(c.comp, c.compLen, out) && out == asBytes(c.raw, c.rawLen));
    }

    // Large vectors pinned by (length, crc32, head/tail).
    for (const auto& meta : kGzipMeta) {
        std::string compName = std::string("kGz_") + meta.name;
        const unsigned char* comp = nullptr;
        unsigned compLen = 0;
        if (compName == "kGz_random8k") {
            comp = kGz_random8k;
            compLen = kGz_random8kLen;
        } else if (compName == "kGz_text100k") {
            comp = kGz_text100k;
            compLen = kGz_text100kLen;
        } else {
            comp = kGz_bomb;
            compLen = kGz_bombLen;
        }
        Bytes out;
        const bool ok = gunzipOk(comp, compLen, out);
        bool match  = ok && out.size() == meta.length && testCrc32(out) == meta.crc;
        if (match && out.size() >= 16) {
            for (int i = 0; i < 16 && match; ++i) {
                match = out[i] == meta.head[i] && out[out.size() - 16 + i] == meta.tail[i];
            }
        }
        check(meta.name, match);
    }

    // Hand-built stored block (no zlib involved on either side).
    {
        const Bytes payload = {'A', 'B'};
        const Bytes member  = storedMember(payload);
        Bytes out;
        check("stored block", gunzipOk(member.data(), static_cast<unsigned>(member.size()), out) && out == payload);
    }

    // Corruption is a loud error.
    {
        Bytes bad = asBytes(kGz_hello, kGz_helloLen);
        bad[0]    = 0x00;
        check("bad magic rejected", gunzipFails(bad.data(), static_cast<unsigned>(bad.size())));
    }
    check("empty input rejected", gunzipFails(nullptr, 0));
    {
        Bytes trunc = asBytes(kGz_hello, kGz_helloLen);
        trunc.resize(trunc.size() / 2);
        check("truncated rejected", gunzipFails(trunc.data(), static_cast<unsigned>(trunc.size())));
    }
    {
        Bytes noTrailer = asBytes(kGz_hello, kGz_helloLen);
        noTrailer.resize(noTrailer.size() - 1);
        check("cut trailer rejected", gunzipFails(noTrailer.data(), static_cast<unsigned>(noTrailer.size())));
    }
    {
        Bytes crcFlip = asBytes(kGz_hello, kGz_helloLen);
        crcFlip[crcFlip.size() - 8] ^= 0xFF;
        check("CRC mismatch rejected", gunzipFails(crcFlip.data(), static_cast<unsigned>(crcFlip.size())));
    }
    {
        Bytes sizeFlip = asBytes(kGz_hello, kGz_helloLen);
        sizeFlip[sizeFlip.size() - 1] ^= 0xFF;
        check("ISIZE mismatch rejected", gunzipFails(sizeFlip.data(), static_cast<unsigned>(sizeFlip.size())));
    }
    check(
        "bomb capped",
        gunzipFails(kGz_bomb, kGz_bombLen, 1024) // 2 MB of zeros must not expand into a 1 KB cap
    );
    {
        // Two concatenated members: strict single-member policy rejects.
        Bytes two = asBytes(kGz_hello, kGz_helloLen);
        two.insert(two.end(), kGz_hello, kGz_hello + kGz_helloLen);
        check("trailing member rejected", gunzipFails(two.data(), static_cast<unsigned>(two.size())));
    }
    {
        // Valid gzip, invalid NBT inside: inflate ok, parse fails loudly.
        Bytes compressed;
        const char inner[] = "this is not nbt";
        // Reuse the stored-block builder, then parse the payload as NBT.
        const Bytes payload(inner, inner + sizeof(inner) - 1);
        compressed = storedMember(payload);
        Bytes out;
        check("non-NBT inflates", gunzipOk(compressed.data(), static_cast<unsigned>(compressed.size()), out));
        const auto parsed = nbt::readRootCompoundFromMemory(out.data(), out.size(), nbt::ByteOrder::Big);
        check("non-NBT parse fails", std::holds_alternative<nbt::ParseError>(parsed));
    }

    // End-to-end: real compressed file -> model (multi-region assertions).
    {
        Bytes file;
        const char* candidates[] = {
            "fixtures/small.litematic",
            "../fixtures/small.litematic",
            "../../fixtures/small.litematic",
            "C:/Projects/Architect/fixtures/small.litematic",
        };
        bool found = false;
        for (const char* c : candidates) {
            if (readFile(c, file)) {
                found = true;
                break;
            }
        }
        check("fixture file found", found);
        if (!found) {
            return failures;
        }
        const schematic::ReadResult r = schematic::readLitematic(file);
        const auto* err               = std::get_if<schematic::ReadError>(&r);
        check("compressed file decodes", err == nullptr);
        if (err) {
            std::printf("  decoder error: %s\n", err->message.c_str());
            return failures;
        }
        const Schematic& s = std::get<Schematic>(r);
        check("two regions", s.regions.size() == 2);
        check("12/7/17 metadata volume", s.enclosedVolume() == 28);
        const Region* test  = nullptr;
        const Region* upper = nullptr;
        for (const auto& region : s.regions) {
            if (region.name == "test") {
                test = &region;
            } else if (region.name == "upper") {
                upper = &region;
            }
        }
        check("test region present", test != nullptr);
        check("upper region present", upper != nullptr);
        if (test && upper) {
            check("test dims", test->size == Vec3i{3, 2, 4});
            check("test pos", test->position == Vec3i{10, 64, -5});
            check("test 24 cells", test->cells.size() == 24);
            check("test straddle idx21", test->cells[21] == 4);
            check("test torch idx23", test->cells[23] == 5);
            check(
                "test stairs states",
                test->palette[2].name == "minecraft:oak_stairs" &&
                    test->palette[2].states.at("facing") == "east"
            );
            check(
                "test chest tile entity",
                test->tileEntities.size() == 1 && test->tileEntities[0].id == "minecraft:chest" &&
                    test->tileEntities[0].pos == Vec3i{0, 0, 3}
            );
            check("upper dims", upper->size == Vec3i{2, 1, 2});
            check("upper pos", upper->position == Vec3i{20, 70, 10});
            check("upper cells [1,3,0,2]", upper->cells == std::vector<std::uint32_t>{1, 3, 0, 2});
            check(
                "upper stairs states",
                upper->palette[1].name == "minecraft:brick_stairs" &&
                    upper->palette[1].states.at("shape") == "inner_left"
            );
            check(
                "upper furnace tile entity",
                upper->tileEntities.size() == 1 && upper->tileEntities[0].id == "minecraft:furnace" &&
                    upper->tileEntities[0].pos == Vec3i{1, 0, 0}
            );
        }
    }
    return failures;
}
