#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace off::data {

enum class TextureEncoding {
    dxt1,
    dxt3,
    abgr32,
    paletted8,
};

struct TextureMip {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::byte> encoded;
};

struct TextureImage {
    std::uint32_t id{0};
    TextureEncoding encoding{TextureEncoding::abgr32};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t format_tag{0};
    std::array<std::uint32_t, 3> metadata{};
    std::string name;
    std::vector<TextureMip> mips;
    std::vector<std::uint32_t> palette;
};

struct TextureSequence {
    std::uint32_t id{0};
    std::vector<std::uint32_t> texture_ids;
};

class TextureCatalog final {
public:
    [[nodiscard]] static TextureCatalog parse(std::span<const std::byte> bytes);

    [[nodiscard]] std::span<const TextureImage> images() const noexcept {
        return images_;
    }
    [[nodiscard]] std::span<const TextureSequence> sequences() const noexcept {
        return sequences_;
    }

private:
    std::vector<TextureImage> images_;
    std::vector<TextureSequence> sequences_;
};

}  // namespace off::data
