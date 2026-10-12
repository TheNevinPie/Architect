#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace architect::gzip {

// Gzip decompression (RFC 1952 container + RFC 1951 raw DEFLATE).
// Hand-rolled, dependency-free, no-throw: the only external input a
// .litematic needs besides the NBT parser, so zlib is NOT required
// (see docs/NBT_INTERFACE.md for the decision record).
struct DecompressError {
    std::string message;
};

using DecompressResult = std::variant<std::vector<std::uint8_t>, DecompressError>;

// Decompress one gzip member. `maxOutputBytes` caps expansion (bomb
// protection): inputs that would expand past it fail with a clear error
// instead of allocating. Trailing bytes after the member are rejected
// (Litematica writes single-member files via GZIPOutputStream).
[[nodiscard]] DecompressResult
decompressGzip(const std::uint8_t* data, std::size_t size, std::size_t maxOutputBytes);

} // namespace architect::gzip
