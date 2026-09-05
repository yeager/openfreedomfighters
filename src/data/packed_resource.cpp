#include "off/data/packed_resource.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <zlib.h>

namespace off::data {
namespace {

constexpr std::size_t header_size = 9;
constexpr std::uint32_t maximum_payload_size = 256U * 1024U * 1024U;

[[nodiscard]] std::uint32_t read_u32_be(
    std::span<const std::byte> bytes,
    std::size_t offset
) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value = (value << 8U) | std::to_integer<std::uint8_t>(bytes[offset + index]);
    }
    return value;
}

}  // namespace

PackedResource PackedResource::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("invalid packed-resource file size");
    }
    const ByteReader reader(bytes);
    const auto payload_size = reader.u32(0);
    const auto file_size = reader.u32(4);
    if (payload_size == 0 || payload_size > maximum_payload_size ||
        file_size != bytes.size()) {
        throw std::runtime_error("invalid packed-resource envelope");
    }

    PackedResource result;
    const auto encoding_value = std::to_integer<std::uint8_t>(bytes[8]);
    if (encoding_value > static_cast<std::uint8_t>(PackedResourceEncoding::stored)) {
        throw std::runtime_error("unsupported packed-resource encoding");
    }
    result.encoding_ = static_cast<PackedResourceEncoding>(encoding_value);
    const auto encoded = bytes.subspan(header_size);
    result.payload_.resize(payload_size);
    if (result.encoding_ == PackedResourceEncoding::stored) {
        if (encoded.size() != result.payload_.size()) {
            throw std::runtime_error("stored packed resource has mismatched sizes");
        }
        std::copy(encoded.begin(), encoded.end(), result.payload_.begin());
        return result;
    }
    if (encoded.size() <= sizeof(std::uint32_t)) {
        throw std::runtime_error("deflated packed resource is missing its checksum");
    }
    const auto compressed = encoded.first(encoded.size() - sizeof(std::uint32_t));
    const auto expected_checksum = read_u32_be(
        bytes,
        bytes.size() - sizeof(std::uint32_t)
    );

    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(result.payload_.data());
    stream.avail_out = static_cast<uInt>(result.payload_.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("could not initialize packed-resource inflater");
    }
    const auto inflate_result = inflate(&stream, Z_FINISH);
    const auto consumed = stream.total_in;
    const auto produced = stream.total_out;
    inflateEnd(&stream);
    const auto actual_checksum = static_cast<std::uint32_t>(::adler32(
        1,
        reinterpret_cast<const Bytef*>(result.payload_.data()),
        static_cast<uInt>(result.payload_.size())
    ));
    if (inflate_result != Z_STREAM_END || consumed != compressed.size() ||
        actual_checksum != expected_checksum ||
        produced != result.payload_.size()) {
        throw std::runtime_error("invalid deflated packed resource");
    }
    return result;
}

}  // namespace off::data
