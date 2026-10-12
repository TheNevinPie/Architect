#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace architect::nbt {

// Minimal NBT model covering exactly what schematic readers need.
// No dependency on any third-party NBT library: the interface is
// vendor-neutral so the backend (mcnbt vs nbt-cpp vs hand-rolled) can be
// swapped without touching readers. See docs/NBT_INTERFACE.md.

enum class TagType : std::uint8_t {
    End = 0,
    Byte = 1,
    Short = 2,
    Int = 3,
    Long = 4,
    Float = 5,
    Double = 6,
    ByteArray = 7,
    String = 8,
    List = 9,
    Compound = 10,
    IntArray = 11,
    LongArray = 12,
};

enum class ByteOrder {
    Big,    // Java schematics: .litematic, .schem (gzip + big-endian NBT)
    Little, // Bedrock: .mcstructure (raw uncompressed little-endian NBT)
};

struct Tag;
using Compound = std::map<std::string, Tag>;
using List     = std::vector<Tag>;

struct Tag {
    TagType type = TagType::End;
    // Payload by type:
    //   Byte: int8_t | Short: int16_t | Int: int32_t | Long: int64_t
    //   Float: float | Double: double | String: std::string
    //   ByteArray: vector<int8_t> | IntArray: vector<int32_t>
    //   LongArray: vector<int64_t> | List: List | Compound: Compound
    std::variant<
        std::monostate,
        std::int8_t,
        std::int16_t,
        std::int32_t,
        std::int64_t,
        float,
        double,
        std::string,
        std::vector<std::int8_t>,
        std::vector<std::int32_t>,
        std::vector<std::int64_t>,
        List,
        Compound>
        value;
};

struct ParseError {
    std::string message;
    std::size_t byteOffset = 0; // where decoding failed, when known
};

// Typed accessors. All return nullopt on type mismatch (never throw;
// the mod builds with exceptions disabled, /EHa + set_exceptions("none")).
[[nodiscard]] std::optional<std::int8_t> asByte(const Tag& tag);
[[nodiscard]] std::optional<std::int16_t> asShort(const Tag& tag);
[[nodiscard]] std::optional<std::int32_t> asInt(const Tag& tag);
[[nodiscard]] std::optional<std::int64_t> asLong(const Tag& tag);
[[nodiscard]] std::optional<float> asFloat(const Tag& tag);
[[nodiscard]] std::optional<double> asDouble(const Tag& tag);
[[nodiscard]] std::optional<const std::string*> asString(const Tag& tag);
[[nodiscard]] std::optional<const std::vector<std::int8_t>*> asByteArray(const Tag& tag);
[[nodiscard]] std::optional<const std::vector<std::int32_t>*> asIntArray(const Tag& tag);
[[nodiscard]] std::optional<const std::vector<std::int64_t>*> asLongArray(const Tag& tag);
[[nodiscard]] std::optional<const List*> asList(const Tag& tag);
[[nodiscard]] std::optional<const Compound*> asCompound(const Tag& tag);

// Look up `key` in a compound; nullopt when absent.
[[nodiscard]] std::optional<const Tag*> find(const Compound& compound, const std::string& key);

// The only two entry points readers use. `readFileBytes` handles the
// transport detected per format (gzip inflate for .litematic/.schem via
// the built-in dependency-free inflater in nbt/Gzip.h; raw for
// .mcstructure). Returns the root compound on success or a ParseError
// describing the first failure.
//
// M2 status: tag parser + gzip inflate are both implemented and
// executed (tests/cpp/TestNbt.cpp, TestGzip.cpp). No zlib needed.
[[nodiscard]] std::variant<Compound, ParseError> readRootCompound(
    const std::vector<std::uint8_t>& fileBytes,
    ByteOrder                        order,
    bool                             gzipCompressed
);

// Decodes one already-in-memory NBT payload (inflated, or raw as in
// .mcstructure) into its root compound. Pure parse: shared by the file
// entry point above and by tests that build buffers by hand.
[[nodiscard]] std::variant<Compound, ParseError> readRootCompoundFromMemory(
    const std::uint8_t* data,
    std::size_t         size,
    ByteOrder           order
);

} // namespace architect::nbt
