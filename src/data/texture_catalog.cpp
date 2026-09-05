#include "off/data/texture_catalog.hpp"

#include "off/data/byte_reader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::data {
namespace {

constexpr std::uint32_t dxt1_tag = 0x44585431U;
constexpr std::uint32_t dxt3_tag = 0x44585433U;
constexpr std::uint32_t abgr_tag = 0x52474241U;
constexpr std::uint32_t palette_tag = 0x50414c4eU;
constexpr std::uint32_t block_size_mask = 0x3fffffffU;
constexpr std::uint32_t block_flag_mask = 0xc0000000U;
constexpr std::size_t header_size = 16;
constexpr std::size_t index_entries = 2048;
constexpr std::size_t index_bytes = index_entries * sizeof(std::uint32_t);
constexpr std::size_t trailing_bytes = index_bytes * 2;
constexpr std::size_t maximum_file_size = 128U * 1024U * 1024U;
constexpr std::uint32_t maximum_dimension = 4096;
constexpr std::uint32_t maximum_mips = 16;
constexpr std::size_t maximum_name_size = 4096;
constexpr std::uint32_t maximum_sequence_entries = 4096;
constexpr std::uint32_t maximum_palette_entries = 256;

struct PendingSequence {
    std::size_t offset{0};
    TextureSequence sequence;
    bool indexed{false};
};

[[nodiscard]] std::optional<TextureEncoding> texture_encoding(std::uint32_t tag) {
    switch (tag) {
        case dxt1_tag:
            return TextureEncoding::dxt1;
        case dxt3_tag:
            return TextureEncoding::dxt3;
        case abgr_tag:
            return TextureEncoding::abgr32;
        case palette_tag:
            return TextureEncoding::paletted8;
        default:
            return std::nullopt;
    }
}

[[nodiscard]] std::uint32_t expected_mip_size(
    TextureEncoding encoding,
    std::uint32_t width,
    std::uint32_t height
) {
    std::uint64_t size = 0;
    switch (encoding) {
        case TextureEncoding::dxt1:
            size = static_cast<std::uint64_t>((width + 3U) / 4U) *
                   ((height + 3U) / 4U) * 8U;
            break;
        case TextureEncoding::dxt3:
            size = static_cast<std::uint64_t>((width + 3U) / 4U) *
                   ((height + 3U) / 4U) * 16U;
            break;
        case TextureEncoding::abgr32:
            size = static_cast<std::uint64_t>(width) * height * 4U;
            break;
        case TextureEncoding::paletted8:
            size = static_cast<std::uint64_t>(width) * height;
            break;
    }
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("texture mip exceeds the supported size");
    }
    return static_cast<std::uint32_t>(size);
}

[[nodiscard]] std::string texture_name(
    std::span<const std::byte> bytes,
    std::size_t& consumed
) {
    const auto terminator = std::find(bytes.begin(), bytes.end(), std::byte{0});
    if (terminator == bytes.end()) {
        throw std::runtime_error("texture name is not NUL terminated");
    }
    const auto length = static_cast<std::size_t>(terminator - bytes.begin());
    if (length > maximum_name_size ||
        !std::all_of(bytes.begin(), terminator, [](std::byte value) {
            const auto character = std::to_integer<unsigned int>(value);
            return character >= 0x20U && character <= 0x7eU;
        })) {
        throw std::runtime_error("texture name is not valid printable ASCII");
    }
    consumed = length + 1;
    return {reinterpret_cast<const char*>(bytes.data()), length};
}

}  // namespace

TextureCatalog TextureCatalog::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < header_size + trailing_bytes ||
        bytes.size() > maximum_file_size) {
        throw std::runtime_error("invalid texture-catalog file size");
    }
    const ByteReader reader(bytes);
    const auto data_end = static_cast<std::size_t>(reader.u32(0));
    const auto sequence_index = static_cast<std::size_t>(reader.u32(4));
    if (reader.u32(8) != 3 || reader.u32(12) != 4 ||
        data_end < header_size || data_end > bytes.size() - trailing_bytes ||
        bytes.size() != data_end + trailing_bytes) {
        throw std::runtime_error("invalid texture-catalog envelope");
    }
    if ((data_end == header_size && sequence_index != header_size) ||
        (data_end != header_size && sequence_index != data_end + index_bytes)) {
        throw std::runtime_error("invalid texture-catalog index offsets");
    }

    TextureCatalog result;
    std::array<std::optional<std::size_t>, index_entries> image_offsets{};
    std::vector<PendingSequence> pending_sequences;
    auto cursor = header_size;
    while (cursor < data_end) {
        if (data_end - cursor < 8) {
            throw std::runtime_error("truncated texture-catalog data block");
        }
        const auto possible_encoding = texture_encoding(reader.u32(cursor + 4));
        if (!possible_encoding.has_value()) {
            const auto count = reader.u32(cursor);
            if (pending_sequences.size() >= index_entries || count == 0 ||
                count > maximum_sequence_entries ||
                static_cast<std::uint64_t>(count) * 4U + 4U > data_end - cursor) {
                throw std::runtime_error("invalid texture-sequence block");
            }
            PendingSequence pending;
            pending.offset = cursor;
            pending.sequence.texture_ids.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto id = reader.u32(cursor + 4U + index * 4U);
                if (id >= index_entries) {
                    throw std::runtime_error("texture sequence contains an invalid image ID");
                }
                pending.sequence.texture_ids.push_back(id);
            }
            pending_sequences.push_back(std::move(pending));
            cursor += 4U + static_cast<std::size_t>(count) * 4U;
            continue;
        }

        const auto descriptor = reader.u32(cursor);
        const auto block_size = static_cast<std::size_t>(descriptor & block_size_mask);
        if ((descriptor & block_flag_mask) != 0 || block_size < 41 ||
            block_size > data_end - cursor) {
            throw std::runtime_error("invalid texture-image block descriptor");
        }
        const auto block_end = cursor + block_size;
        const auto content = cursor + 4;
        const auto format_tag = reader.u32(content);
        if (reader.u32(content + 4) != format_tag) {
            throw std::runtime_error("texture-image format tags disagree");
        }
        const auto encoding = texture_encoding(format_tag);
        if (!encoding.has_value()) {
            throw std::runtime_error("unsupported texture-image format");
        }
        const auto id = reader.u32(content + 8);
        if (id >= index_entries || image_offsets[id].has_value()) {
            throw std::runtime_error("duplicate or invalid texture-image ID");
        }
        const auto dimensions = reader.u32(content + 12);
        const auto width = dimensions & 0xffffU;
        const auto height = dimensions >> 16U;
        const auto mip_count = reader.u32(content + 16);
        if (width == 0 || height == 0 || width > maximum_dimension ||
            height > maximum_dimension || mip_count == 0 || mip_count > maximum_mips) {
            throw std::runtime_error("invalid texture-image dimensions or mip count");
        }

        TextureImage image;
        image.id = id;
        image.encoding = *encoding;
        image.width = width;
        image.height = height;
        image.format_tag = format_tag;
        image.metadata = {
            reader.u32(content + 20),
            reader.u32(content + 24),
            reader.u32(content + 28),
        };
        const auto name_start = content + 32;
        std::size_t name_bytes = 0;
        image.name = texture_name(reader.slice(name_start, block_end - name_start), name_bytes);
        auto position = name_start + name_bytes;
        auto mip_width = width;
        auto mip_height = height;
        image.mips.reserve(mip_count);
        for (std::uint32_t level = 0; level < mip_count; ++level) {
            if (block_end - position < 4) {
                throw std::runtime_error("truncated texture mip header");
            }
            const auto encoded_size = reader.u32(position);
            position += 4;
            const auto expected = expected_mip_size(*encoding, mip_width, mip_height);
            if (encoded_size != expected || encoded_size > block_end - position) {
                throw std::runtime_error("texture mip size does not match its format");
            }
            const auto encoded = reader.slice(position, encoded_size);
            image.mips.push_back({
                .width = mip_width,
                .height = mip_height,
                .encoded = {encoded.begin(), encoded.end()},
            });
            position += encoded_size;
            mip_width = std::max(1U, mip_width / 2U);
            mip_height = std::max(1U, mip_height / 2U);
        }
        if (*encoding == TextureEncoding::paletted8) {
            if (block_end - position < 4) {
                throw std::runtime_error("truncated texture palette header");
            }
            const auto palette_count = reader.u32(position);
            position += 4;
            if (palette_count == 0 || palette_count > maximum_palette_entries ||
                static_cast<std::uint64_t>(palette_count) * 4U != block_end - position) {
                throw std::runtime_error("invalid texture palette size");
            }
            image.palette.reserve(palette_count);
            for (std::uint32_t index = 0; index < palette_count; ++index) {
                image.palette.push_back(reader.u32(position + index * 4U));
            }
            position += static_cast<std::size_t>(palette_count) * 4U;
            for (const auto& mip : image.mips) {
                if (!std::all_of(mip.encoded.begin(), mip.encoded.end(), [palette_count](
                        std::byte value
                    ) {
                        return std::to_integer<std::uint32_t>(value) < palette_count;
                    })) {
                    throw std::runtime_error("texture mip contains an invalid palette index");
                }
            }
        }
        if (position != block_end) {
            throw std::runtime_error("texture-image block has trailing data");
        }
        image_offsets[id] = cursor;
        result.images_.push_back(std::move(image));
        cursor = block_end;
    }

    for (std::size_t id = 0; id < index_entries; ++id) {
        const auto indexed_offset = static_cast<std::size_t>(
            reader.u32(data_end + id * sizeof(std::uint32_t))
        );
        const auto expected = image_offsets[id].value_or(0);
        if (indexed_offset != expected) {
            throw std::runtime_error("texture-image index does not match data blocks");
        }
    }

    for (std::size_t id = 0; id < index_entries; ++id) {
        const auto indexed_offset = static_cast<std::size_t>(
            reader.u32(sequence_index + id * sizeof(std::uint32_t))
        );
        if (indexed_offset == 0) {
            continue;
        }
        const auto found = std::find_if(
            pending_sequences.begin(),
            pending_sequences.end(),
            [indexed_offset](const PendingSequence& pending) {
                return pending.offset == indexed_offset;
            }
        );
        if (found == pending_sequences.end() || found->indexed) {
            throw std::runtime_error("texture-sequence index does not match data blocks");
        }
        if (std::find(found->sequence.texture_ids.begin(), found->sequence.texture_ids.end(), id) ==
            found->sequence.texture_ids.end()) {
            throw std::runtime_error("texture sequence does not contain its indexed ID");
        }
        for (const auto texture_id : found->sequence.texture_ids) {
            if (!image_offsets[texture_id].has_value()) {
                throw std::runtime_error("texture sequence references a missing image");
            }
        }
        found->indexed = true;
        found->sequence.id = static_cast<std::uint32_t>(id);
    }
    if (!std::all_of(
            pending_sequences.begin(),
            pending_sequences.end(),
            [](const PendingSequence& pending) { return pending.indexed; }
        )) {
        throw std::runtime_error("texture-sequence block is absent from its index");
    }
    result.sequences_.reserve(pending_sequences.size());
    for (auto& pending : pending_sequences) {
        result.sequences_.push_back(std::move(pending.sequence));
    }
    return result;
}

}  // namespace off::data
