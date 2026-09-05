#include "off/graphics/texture_decode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace off::graphics {
namespace {

struct Color {
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    std::uint8_t alpha{255};
};

[[nodiscard]] std::uint16_t little_u16(std::span<const std::byte> bytes) {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(bytes[0]) |
        (std::to_integer<std::uint16_t>(bytes[1]) << 8U)
    );
}

[[nodiscard]] std::uint32_t little_u32(std::span<const std::byte> bytes) {
    return std::to_integer<std::uint32_t>(bytes[0]) |
           (std::to_integer<std::uint32_t>(bytes[1]) << 8U) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[3]) << 24U);
}

[[nodiscard]] std::uint64_t little_u64(std::span<const std::byte> bytes) {
    std::uint64_t value = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        value |= std::to_integer<std::uint64_t>(bytes[index]) << (index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint8_t expand5(std::uint16_t value) {
    return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
}

[[nodiscard]] std::uint8_t expand6(std::uint16_t value) {
    return static_cast<std::uint8_t>((value << 2U) | (value >> 4U));
}

[[nodiscard]] Color rgb565(std::uint16_t value) {
    return {
        .red = expand5(static_cast<std::uint16_t>((value >> 11U) & 0x1fU)),
        .green = expand6(static_cast<std::uint16_t>((value >> 5U) & 0x3fU)),
        .blue = expand5(static_cast<std::uint16_t>(value & 0x1fU)),
        .alpha = 255,
    };
}

[[nodiscard]] Color interpolate(Color first, Color second, unsigned int first_weight) {
    const auto second_weight = 3U - first_weight;
    return {
        .red = static_cast<std::uint8_t>(
            (first_weight * first.red + second_weight * second.red) / 3U
        ),
        .green = static_cast<std::uint8_t>(
            (first_weight * first.green + second_weight * second.green) / 3U
        ),
        .blue = static_cast<std::uint8_t>(
            (first_weight * first.blue + second_weight * second.blue) / 3U
        ),
        .alpha = 255,
    };
}

void write_pixel(
    RgbaImage& output,
    std::uint32_t x,
    std::uint32_t y,
    Color color
) {
    if (x >= output.width || y >= output.height) {
        return;
    }
    const auto offset = (static_cast<std::size_t>(y) * output.width + x) * 4U;
    output.pixels[offset] = color.red;
    output.pixels[offset + 1] = color.green;
    output.pixels[offset + 2] = color.blue;
    output.pixels[offset + 3] = color.alpha;
}

[[nodiscard]] RgbaImage allocate_output(std::uint32_t width, std::uint32_t height) {
    const auto pixel_count = static_cast<std::uint64_t>(width) * height;
    if (width == 0 || height == 0 ||
        pixel_count > std::numeric_limits<std::size_t>::max() / 4U ||
        pixel_count > 16U * 1024U * 1024U) {
        throw std::runtime_error("decoded texture dimensions exceed the safety limit");
    }
    return {
        .width = width,
        .height = height,
        .pixels = std::vector<std::uint8_t>(static_cast<std::size_t>(pixel_count) * 4U),
    };
}

[[nodiscard]] std::size_t block_encoded_size(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t bytes_per_block
) {
    const auto blocks_wide = (static_cast<std::uint64_t>(width) + 3U) / 4U;
    const auto blocks_high = (static_cast<std::uint64_t>(height) + 3U) / 4U;
    const auto block_count = blocks_wide * blocks_high;
    if (block_count > std::numeric_limits<std::size_t>::max() / bytes_per_block) {
        throw std::runtime_error("compressed texture mip exceeds the safety limit");
    }
    return static_cast<std::size_t>(block_count) * bytes_per_block;
}

[[nodiscard]] RgbaImage decode_blocks(
    const data::TextureMip& mip,
    bool explicit_alpha
) {
    const auto bytes_per_block = explicit_alpha ? 16U : 8U;
    auto output = allocate_output(mip.width, mip.height);
    if (mip.encoded.size() != block_encoded_size(mip.width, mip.height, bytes_per_block)) {
        throw std::runtime_error("compressed texture mip has an invalid byte count");
    }
    const auto blocks_wide = (mip.width + 3U) / 4U;
    const auto blocks_high = (mip.height + 3U) / 4U;
    for (std::uint32_t block_y = 0; block_y < blocks_high; ++block_y) {
        for (std::uint32_t block_x = 0; block_x < blocks_wide; ++block_x) {
            const auto block_index = static_cast<std::size_t>(block_y) * blocks_wide + block_x;
            const auto block = std::span<const std::byte>{mip.encoded}.subspan(
                block_index * bytes_per_block,
                bytes_per_block
            );
            const auto color_offset = explicit_alpha ? 8U : 0U;
            const auto first_565 = little_u16(block.subspan(color_offset, 2));
            const auto second_565 = little_u16(block.subspan(color_offset + 2, 2));
            std::array colors{
                rgb565(first_565),
                rgb565(second_565),
                Color{},
                Color{},
            };
            if (explicit_alpha || first_565 > second_565) {
                colors[2] = interpolate(colors[0], colors[1], 2);
                colors[3] = interpolate(colors[0], colors[1], 1);
            } else {
                colors[2] = {
                    .red = static_cast<std::uint8_t>(
                        (static_cast<unsigned int>(colors[0].red) + colors[1].red) / 2U
                    ),
                    .green = static_cast<std::uint8_t>(
                        (static_cast<unsigned int>(colors[0].green) + colors[1].green) / 2U
                    ),
                    .blue = static_cast<std::uint8_t>(
                        (static_cast<unsigned int>(colors[0].blue) + colors[1].blue) / 2U
                    ),
                    .alpha = 255,
                };
                colors[3] = {.red = 0, .green = 0, .blue = 0, .alpha = 0};
            }
            const auto color_indexes = little_u32(block.subspan(color_offset + 4, 4));
            const auto alpha_values = explicit_alpha ? little_u64(block.first(8)) : 0;
            for (std::uint32_t pixel = 0; pixel < 16; ++pixel) {
                auto color = colors[(color_indexes >> (pixel * 2U)) & 3U];
                if (explicit_alpha) {
                    color.alpha = static_cast<std::uint8_t>(
                        ((alpha_values >> (pixel * 4U)) & 0xfU) * 17U
                    );
                }
                write_pixel(
                    output,
                    block_x * 4U + pixel % 4U,
                    block_y * 4U + pixel / 4U,
                    color
                );
            }
        }
    }
    return output;
}

[[nodiscard]] Color packed_color(std::uint32_t value) {
    return {
        .red = static_cast<std::uint8_t>((value >> 16U) & 0xffU),
        .green = static_cast<std::uint8_t>((value >> 8U) & 0xffU),
        .blue = static_cast<std::uint8_t>(value & 0xffU),
        .alpha = static_cast<std::uint8_t>(value >> 24U),
    };
}

}  // namespace

RgbaImage decode_texture_mip(const data::TextureImage& texture, std::size_t mip_level) {
    if (mip_level >= texture.mips.size()) {
        throw std::runtime_error("texture mip level is out of range");
    }
    const auto& mip = texture.mips[mip_level];
    switch (texture.encoding) {
        case data::TextureEncoding::dxt1:
            return decode_blocks(mip, false);
        case data::TextureEncoding::dxt3:
            return decode_blocks(mip, true);
        case data::TextureEncoding::abgr32: {
            const auto pixel_count = static_cast<std::uint64_t>(mip.width) * mip.height;
            if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U ||
                mip.encoded.size() != pixel_count * 4U) {
                throw std::runtime_error("ABGR texture mip has an invalid byte count");
            }
            auto output = allocate_output(mip.width, mip.height);
            for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(pixel_count); ++pixel) {
                const auto source = pixel * 4U;
                output.pixels[source] = std::to_integer<std::uint8_t>(mip.encoded[source + 2]);
                output.pixels[source + 1] = std::to_integer<std::uint8_t>(mip.encoded[source + 1]);
                output.pixels[source + 2] = std::to_integer<std::uint8_t>(mip.encoded[source]);
                output.pixels[source + 3] = std::to_integer<std::uint8_t>(mip.encoded[source + 3]);
            }
            return output;
        }
        case data::TextureEncoding::paletted8: {
            const auto pixel_count = static_cast<std::uint64_t>(mip.width) * mip.height;
            if (pixel_count > std::numeric_limits<std::size_t>::max() ||
                mip.encoded.size() != pixel_count || texture.palette.empty() ||
                texture.palette.size() > 256) {
                throw std::runtime_error("paletted texture mip has invalid storage");
            }
            auto output = allocate_output(mip.width, mip.height);
            for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(pixel_count); ++pixel) {
                const auto index = std::to_integer<std::uint8_t>(mip.encoded[pixel]);
                if (index >= texture.palette.size()) {
                    throw std::runtime_error("paletted texture mip contains an invalid index");
                }
                const auto color = packed_color(texture.palette[index]);
                const auto target = pixel * 4U;
                output.pixels[target] = color.red;
                output.pixels[target + 1] = color.green;
                output.pixels[target + 2] = color.blue;
                output.pixels[target + 3] = color.alpha;
            }
            return output;
        }
    }
    throw std::runtime_error("unsupported texture encoding");
}

}  // namespace off::graphics
