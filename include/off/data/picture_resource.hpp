#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

struct PictureResourceDescriptor {
    std::array<std::byte, 40> encoded{};
};

struct PictureResourceFrame {
    std::uint32_t opaque_value{0};
    std::size_t descriptor_index{0};
};

class PictureResource final {
public:
    [[nodiscard]] static PictureResource parse(
        std::span<const std::byte> allocation,
        std::uint32_t relocation_key
    );

    [[nodiscard]] std::span<const PictureResourceDescriptor> descriptors() const
        noexcept {
        return descriptors_;
    }
    [[nodiscard]] std::span<const std::uint32_t>
    frame_texture_references() const noexcept {
        return frame_texture_references_;
    }
    [[nodiscard]] std::span<const PictureResourceFrame> frames() const noexcept {
        return frames_;
    }
    [[nodiscard]] std::size_t encoded_size() const noexcept {
        return encoded_size_;
    }

private:
    std::vector<PictureResourceDescriptor> descriptors_;
    std::vector<std::uint32_t> frame_texture_references_;
    std::vector<PictureResourceFrame> frames_;
    std::size_t encoded_size_{0};
};

}  // namespace off::data
