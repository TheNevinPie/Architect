// M2: hand-rolled tag parser (big- + little-endian), no exceptions,
// no third-party dependency. gzip inflate still needs zlib via xmake
// (see readRootCompound below + docs/NBT_INTERFACE.md).
#include "architect/nbt/Nbt.h"

#include "architect/nbt/Gzip.h"

#include <cstring>

namespace architect::nbt {
// Defined below; declared here for the vector entry point.
std::variant<Compound, ParseError> readRootCompoundFromMemory(const std::uint8_t*, std::size_t, ByteOrder);
} // namespace architect::nbt

namespace architect::nbt {

// Bomb protection: refuses inputs that would expand past this.
// A 256^3 ghost is ~16.7M cells; its NBT form stays far below this cap,
// and schematic-sized files never legitimately approach it.
constexpr std::size_t kMaxDecompressedBytes = 256ULL << 20;

std::optional<std::int8_t> asByte(const Tag& tag) {
    if (tag.type != TagType::Byte) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::int8_t>(&tag.value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<std::int16_t> asShort(const Tag& tag) {
    if (tag.type != TagType::Short) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::int16_t>(&tag.value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<std::int32_t> asInt(const Tag& tag) {
    if (tag.type != TagType::Int) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::int32_t>(&tag.value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<std::int64_t> asLong(const Tag& tag) {
    if (tag.type != TagType::Long) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::int64_t>(&tag.value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<float> asFloat(const Tag& tag) {
    if (tag.type != TagType::Float) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<float>(&tag.value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<double> asDouble(const Tag& tag) {
    if (tag.type != TagType::Double) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<double>(&tag.value)) {
        return *v;
    }
    return std::nullopt;
}

std::optional<const std::string*> asString(const Tag& tag) {
    if (tag.type != TagType::String) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::string>(&tag.value)) {
        return v;
    }
    return std::nullopt;
}

std::optional<const std::vector<std::int8_t>*> asByteArray(const Tag& tag) {
    if (tag.type != TagType::ByteArray) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::vector<std::int8_t>>(&tag.value)) {
        return v;
    }
    return std::nullopt;
}

std::optional<const std::vector<std::int32_t>*> asIntArray(const Tag& tag) {
    if (tag.type != TagType::IntArray) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::vector<std::int32_t>>(&tag.value)) {
        return v;
    }
    return std::nullopt;
}

std::optional<const std::vector<std::int64_t>*> asLongArray(const Tag& tag) {
    if (tag.type != TagType::LongArray) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<std::vector<std::int64_t>>(&tag.value)) {
        return v;
    }
    return std::nullopt;
}

std::optional<const List*> asList(const Tag& tag) {
    if (tag.type != TagType::List) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<List>(&tag.value)) {
        return v;
    }
    return std::nullopt;
}

std::optional<const Compound*> asCompound(const Tag& tag) {
    if (tag.type != TagType::Compound) {
        return std::nullopt;
    }
    if (const auto* v = std::get_if<Compound>(&tag.value)) {
        return v;
    }
    return std::nullopt;
}

std::optional<const Tag*> find(const Compound& compound, const std::string& key) {
    const auto it = compound.find(key);
    if (it == compound.end()) {
        return std::nullopt;
    }
    return &it->second;
}

std::variant<Compound, ParseError> readRootCompound(
    const std::vector<std::uint8_t>& fileBytes,
    ByteOrder                        order,
    bool                             gzipCompressed
) {
    if (gzipCompressed) {
        const auto inflated =
            gzip::decompressGzip(fileBytes.data(), fileBytes.size(), kMaxDecompressedBytes);
        if (const auto* err = std::get_if<gzip::DecompressError>(&inflated)) {
            return ParseError{"gzip: " + err->message};
        }
        const auto& raw = std::get<std::vector<std::uint8_t>>(inflated);
        return readRootCompoundFromMemory(raw.data(), raw.size(), order);
    }
    // Raw uncompressed payload (.mcstructure): parse in place.
    return readRootCompoundFromMemory(fileBytes.data(), fileBytes.size(), order);
}

namespace {

// Bounded cursor: every read checks the end first, so corrupt input
// yields ParseError instead of over-reads. No exceptions anywhere.
class Cursor {
public:
    Cursor(const std::uint8_t* data, std::size_t size, ByteOrder order)
        : mPtr(data), mEnd(data + size), mOrder(order) {}

    [[nodiscard]] std::size_t offset(const std::uint8_t* base) const { return static_cast<std::size_t>(mPtr - base); }
    [[nodiscard]] bool ok() const { return mOk; }
    [[nodiscard]] std::size_t remaining() const { return static_cast<std::size_t>(mEnd - mPtr); }

    std::uint8_t u8() {
        if (!take(1)) {
            return 0;
        }
        return *mPtr++;
    }

    std::int8_t i8() { return static_cast<std::int8_t>(u8()); }

    std::uint16_t u16() {
        if (!take(2)) {
            return 0;
        }
        std::uint16_t v;
        if (mOrder == ByteOrder::Big) {
            v = static_cast<std::uint16_t>((static_cast<std::uint16_t>(mPtr[0]) << 8) | mPtr[1]);
        } else {
            v = static_cast<std::uint16_t>(mPtr[0] | (static_cast<std::uint16_t>(mPtr[1]) << 8));
        }
        mPtr += 2;
        return v;
    }

    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }

    std::uint32_t u32() {
        if (!take(4)) {
            return 0;
        }
        std::uint32_t v;
        if (mOrder == ByteOrder::Big) {
            v = (static_cast<std::uint32_t>(mPtr[0]) << 24) | (static_cast<std::uint32_t>(mPtr[1]) << 16) |
                (static_cast<std::uint32_t>(mPtr[2]) << 8) | mPtr[3];
        } else {
            v = mPtr[0] | (static_cast<std::uint32_t>(mPtr[1]) << 8) |
                (static_cast<std::uint32_t>(mPtr[2]) << 16) | (static_cast<std::uint32_t>(mPtr[3]) << 24);
        }
        mPtr += 4;
        return v;
    }

    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

    std::uint64_t u64() {
        if (!take(8)) {
            return 0;
        }
        std::uint64_t v = 0;
        if (mOrder == ByteOrder::Big) {
            for (int i = 0; i < 8; ++i) {
                v = (v << 8) | mPtr[i];
            }
        } else {
            for (int i = 7; i >= 0; --i) {
                v = (v << 8) | mPtr[i];
            }
        }
        mPtr += 8;
        return v;
    }

    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }

    float f32() {
        const std::uint32_t u = u32();
        float f = 0.0F;
        std::memcpy(&f, &u, sizeof(f));
        return f;
    }

    double f64() {
        const std::uint64_t u = u64();
        double d = 0.0;
        std::memcpy(&d, &u, sizeof(d));
        return d;
    }

    // Returns false when the length prefix or bytes are missing.
    bool readString(std::string& out) {
        const std::uint16_t len = u16();
        if (!mOk || !take(len)) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(mPtr), len);
        mPtr += len;
        return true;
    }

private:
    bool take(std::size_t n) {
        if (!mOk || remaining() < n) {
            mOk = false;
            return false;
        }
        return true;
    }

    const std::uint8_t* mPtr;
    const std::uint8_t* mEnd;
    ByteOrder mOrder;
    bool mOk = true;
};

constexpr int kMaxDepth = 64;
constexpr std::size_t kMaxArrayLen = 1ULL << 28; // 256M entries sanity cap

bool parsePayload(Cursor& cur, TagType type, Tag& out, int depth, const std::uint8_t* base, ParseError& err);

bool parseCompoundBody(Cursor& cur, Compound& out, int depth, const std::uint8_t* base, ParseError& err) {
    if (depth > kMaxDepth) {
        err = ParseError{"NBT nesting exceeds 64 levels", cur.offset(base)};
        return false;
    }
    for (;;) {
        const auto entryType = static_cast<TagType>(cur.u8());
        if (!cur.ok()) {
            err = ParseError{"truncated NBT inside compound", cur.offset(base)};
            return false;
        }
        if (entryType == TagType::End) {
            return true;
        }
        std::string name;
        if (!cur.readString(name)) {
            err = ParseError{"truncated NBT tag name", cur.offset(base)};
            return false;
        }
        Tag tag;
        tag.type = entryType;
        if (!parsePayload(cur, entryType, tag, depth + 1, base, err)) {
            return false;
        }
        out.emplace(std::move(name), std::move(tag));
    }
}

bool parseListBody(Cursor& cur, TagType elem, std::size_t count, List& out, int depth, const std::uint8_t* base, ParseError& err) {
    if (depth > kMaxDepth) {
        err = ParseError{"NBT nesting exceeds 64 levels", cur.offset(base)};
        return false;
    }
    if (elem == TagType::End && count != 0) {
        err = ParseError{"non-empty list of TAG_End", cur.offset(base)};
        return false;
    }
    if (count > cur.remaining()) {
        // Every list item costs >= 1 byte; a bigger count is corrupt input.
        err = ParseError{"TAG_List length exceeds remaining bytes", cur.offset(base)};
        return false;
    }
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        Tag tag;
        tag.type = elem;
        if (!parsePayload(cur, elem, tag, depth + 1, base, err)) {
            return false;
        }
        out.push_back(std::move(tag));
    }
    return true;
}

bool parsePayload(Cursor& cur, TagType type, Tag& out, int depth, const std::uint8_t* base, ParseError& err) {
    switch (type) {
        case TagType::End:
            err = ParseError{"unexpected TAG_End as payload", cur.offset(base)};
            return false;
        case TagType::Byte:
            out.value = cur.i8();
            break;
        case TagType::Short:
            out.value = cur.i16();
            break;
        case TagType::Int:
            out.value = cur.i32();
            break;
        case TagType::Long:
            out.value = cur.i64();
            break;
        case TagType::Float:
            out.value = cur.f32();
            break;
        case TagType::Double:
            out.value = cur.f64();
            break;
        case TagType::String: {
            std::string s;
            if (!cur.readString(s)) {
                err = ParseError{"truncated NBT string", cur.offset(base)};
                return false;
            }
            out.value = std::move(s);
            break;
        }
        case TagType::ByteArray: {
            const auto rawLen = cur.i32();
            if (!cur.ok() || rawLen < 0 || static_cast<std::uint64_t>(rawLen) > kMaxArrayLen) {
                err = ParseError{"bad TAG_Byte_Array length", cur.offset(base)};
                return false;
            }
            const auto len = static_cast<std::size_t>(rawLen);
            if (cur.remaining() < len) {
                err = ParseError{"truncated TAG_Byte_Array", cur.offset(base)};
                return false;
            }
            std::vector<std::int8_t> v;
            v.reserve(len);
            for (std::size_t i = 0; i < len; ++i) {
                v.push_back(cur.i8());
            }
            out.value = std::move(v);
            break;
        }
        case TagType::List: {
            const auto elem = static_cast<TagType>(cur.u8());
            const auto rawLen = cur.i32();
            if (!cur.ok() || rawLen < 0 || static_cast<std::uint64_t>(rawLen) > kMaxArrayLen) {
                err = ParseError{"bad TAG_List length", cur.offset(base)};
                return false;
            }
            List list;
            if (!parseListBody(cur, elem, static_cast<std::size_t>(rawLen), list, depth, base, err)) {
                return false;
            }
            out.value = std::move(list);
            break;
        }
        case TagType::Compound: {
            Compound c;
            if (!parseCompoundBody(cur, c, depth, base, err)) {
                return false;
            }
            out.value = std::move(c);
            break;
        }
        case TagType::IntArray: {
            const auto rawLen = cur.i32();
            if (!cur.ok() || rawLen < 0 || static_cast<std::uint64_t>(rawLen) > kMaxArrayLen) {
                err = ParseError{"bad TAG_Int_Array length", cur.offset(base)};
                return false;
            }
            const auto len = static_cast<std::size_t>(rawLen);
            if (cur.remaining() < len * 4) {
                err = ParseError{"truncated TAG_Int_Array", cur.offset(base)};
                return false;
            }
            std::vector<std::int32_t> v;
            v.reserve(len);
            for (std::size_t i = 0; i < len; ++i) {
                v.push_back(cur.i32());
            }
            out.value = std::move(v);
            break;
        }
        case TagType::LongArray: {
            const auto rawLen = cur.i32();
            if (!cur.ok() || rawLen < 0 || static_cast<std::uint64_t>(rawLen) > kMaxArrayLen) {
                err = ParseError{"bad TAG_Long_Array length", cur.offset(base)};
                return false;
            }
            const auto len = static_cast<std::size_t>(rawLen);
            if (cur.remaining() < len * 8) {
                err = ParseError{"truncated TAG_Long_Array", cur.offset(base)};
                return false;
            }
            std::vector<std::int64_t> v;
            v.reserve(len);
            for (std::size_t i = 0; i < len; ++i) {
                v.push_back(cur.i64());
            }
            out.value = std::move(v);
            break;
        }
        default:
            err = ParseError{"unknown NBT tag id", cur.offset(base)};
            return false;
    }
    if (!cur.ok()) {
        err = ParseError{"truncated NBT payload", cur.offset(base)};
        return false;
    }
    return true;
}

} // namespace

std::variant<Compound, ParseError> readRootCompoundFromMemory(
    const std::uint8_t* data,
    std::size_t         size,
    ByteOrder           order
) {
    if (data == nullptr && size != 0) {
        return ParseError{"null NBT buffer"};
    }
    Cursor cur(data == nullptr ? reinterpret_cast<const std::uint8_t*>("") : data, size, order);
    const auto rootType = static_cast<TagType>(cur.u8());
    if (!cur.ok()) {
        return ParseError{"empty NBT buffer"};
    }
    if (rootType != TagType::Compound) {
        return ParseError{"NBT root is not a compound"};
    }
    std::string rootName;
    if (!cur.readString(rootName)) {
        return ParseError{"truncated NBT root name"};
    }
    Compound root;
    ParseError err;
    if (!parseCompoundBody(cur, root, 0, data == nullptr ? reinterpret_cast<const std::uint8_t*>("") : data, err)) {
        return err;
    }
    if (cur.remaining() != 0) {
        ParseError trailing;
        trailing.message = "trailing bytes after NBT root compound";
        trailing.byteOffset = size - cur.remaining();
        return trailing;
    }
    return root;
}

} // namespace architect::nbt
