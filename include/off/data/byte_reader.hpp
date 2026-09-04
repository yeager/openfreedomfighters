#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

class ByteReader final {
public:
    explicit ByteReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const {
        require(offset, 2);
        return static_cast<std::uint16_t>(
            std::to_integer<std::uint16_t>(bytes_[offset]) |
            (std::to_integer<std::uint16_t>(bytes_[offset + 1]) << 8U)
        );
    }

    [[nodiscard]] std::uint32_t u32(std::size_t offset) const {
        require(offset, 4);
        return std::to_integer<std::uint32_t>(bytes_[offset]) |
               (std::to_integer<std::uint32_t>(bytes_[offset + 1]) << 8U) |
               (std::to_integer<std::uint32_t>(bytes_[offset + 2]) << 16U) |
               (std::to_integer<std::uint32_t>(bytes_[offset + 3]) << 24U);
    }

    [[nodiscard]] std::span<const std::byte> slice(
        std::size_t offset,
        std::size_t size
    ) const {
        require(offset, size);
        return bytes_.subspan(offset, size);
    }

private:
    void require(std::size_t offset, std::size_t size) const {
        if (offset > bytes_.size() || size > bytes_.size() - offset) {
            throw std::runtime_error("binary field exceeds input bounds");
        }
    }

    std::span<const std::byte> bytes_;
};

}  // namespace off::data
