#include "off/graphics/texture_decode.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::vector<std::byte> bytes(std::initializer_list<unsigned int> values) {
    std::vector<std::byte> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

off::data::TextureImage texture(
    off::data::TextureEncoding encoding,
    std::uint32_t width,
    std::uint32_t height,
    std::vector<std::byte> encoded
) {
    off::data::TextureImage result;
    result.encoding = encoding;
    result.width = width;
    result.height = height;
    result.mips.push_back({
        .width = width,
        .height = height,
        .encoded = std::move(encoded),
    });
    return result;
}

template <typename Operation>
void check_rejected(Operation operation, const char* message) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, message);
}

}  // namespace

int main() {
    const auto abgr = off::graphics::decode_texture_mip(
        texture(off::data::TextureEncoding::abgr32, 1, 1, bytes({0x33, 0x22, 0x11, 0x80})),
        0
    );
    check(abgr.width == 1 && abgr.height == 1, "preserve ABGR dimensions");
    check(abgr.pixels == std::vector<std::uint8_t>{0x11, 0x22, 0x33, 0x80},
          "convert ABGR storage to RGBA8");

    auto paletted = texture(
        off::data::TextureEncoding::paletted8,
        2,
        1,
        bytes({0, 1})
    );
    paletted.palette = {0xff112233U, 0x80445566U};
    const auto palette_output = off::graphics::decode_texture_mip(paletted, 0);
    check(
        palette_output.pixels == std::vector<std::uint8_t>{
            0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0x80,
        },
        "expand packed palette colors to RGBA8"
    );

    const auto dxt1 = off::graphics::decode_texture_mip(
        texture(
            off::data::TextureEncoding::dxt1,
            4,
            4,
            bytes({0x00, 0xf8, 0xe0, 0x07, 0xe4, 0xe4, 0xe4, 0xe4})
        ),
        0
    );
    check(
        std::vector<std::uint8_t>(dxt1.pixels.begin(), dxt1.pixels.begin() + 16) ==
            std::vector<std::uint8_t>{
                255, 0, 0, 255, 0, 255, 0, 255,
                170, 85, 0, 255, 85, 170, 0, 255,
            },
        "decode DXT1 four-color interpolation"
    );

    const auto dxt1_transparent = off::graphics::decode_texture_mip(
        texture(
            off::data::TextureEncoding::dxt1,
            4,
            4,
            bytes({0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff})
        ),
        0
    );
    check(dxt1_transparent.pixels[3] == 0, "decode DXT1 transparent selector");

    const auto dxt3 = off::graphics::decode_texture_mip(
        texture(
            off::data::TextureEncoding::dxt3,
            4,
            4,
            bytes({
                0x50, 0xfa, 0x50, 0xfa, 0x50, 0xfa, 0x50, 0xfa,
                0x00, 0x00, 0xff, 0xff, 0xe4, 0xe4, 0xe4, 0xe4,
            })
        ),
        0
    );
    check(
        std::vector<std::uint8_t>(dxt3.pixels.begin(), dxt3.pixels.begin() + 16) ==
            std::vector<std::uint8_t>{
                0, 0, 0, 0, 255, 255, 255, 85,
                85, 85, 85, 170, 170, 170, 170, 255,
            },
        "decode DXT3 color and explicit alpha"
    );

    const auto clipped = off::graphics::decode_texture_mip(
        texture(
            off::data::TextureEncoding::dxt1,
            2,
            2,
            bytes({0x00, 0xf8, 0xe0, 0x07, 0xe4, 0xe4, 0xe4, 0xe4})
        ),
        0
    );
    check(clipped.pixels.size() == 2 * 2 * 4, "clip compressed edge blocks");

    check_rejected(
        [&] { static_cast<void>(off::graphics::decode_texture_mip(paletted, 1)); },
        "reject an unavailable mip level"
    );
    check_rejected(
        [] {
            static_cast<void>(off::graphics::decode_texture_mip(
                texture(off::data::TextureEncoding::dxt1, 4, 4, bytes({0, 0, 0, 0})),
                0
            ));
        },
        "reject truncated compressed storage"
    );
    check_rejected(
        [] {
            static_cast<void>(off::graphics::decode_texture_mip(
                texture(off::data::TextureEncoding::abgr32, 1, 1, bytes({0, 0, 0})),
                0
            ));
        },
        "reject truncated ABGR storage"
    );
    check_rejected(
        [] {
            auto value = texture(off::data::TextureEncoding::paletted8, 1, 1, bytes({1}));
            value.palette = {0xff000000U};
            static_cast<void>(off::graphics::decode_texture_mip(value, 0));
        },
        "reject an invalid palette index"
    );
    check_rejected(
        [] {
            const auto value = texture(
                off::data::TextureEncoding::paletted8,
                1,
                1,
                bytes({0})
            );
            static_cast<void>(off::graphics::decode_texture_mip(value, 0));
        },
        "reject a missing palette"
    );

    return failures == 0 ? 0 : 1;
}
