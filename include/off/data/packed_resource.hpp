#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::data {

enum class PackedResourceEncoding : std::uint8_t {
    deflate = 0,
    stored = 1,
};

class PackedResource final {
public:
    [[nodiscard]] static PackedResource parse(std::span<const std::byte> bytes);

    [[nodiscard]] PackedResourceEncoding encoding() const noexcept {
        return encoding_;
    }
    [[nodiscard]] std::span<const std::byte> payload() const noexcept {
        return payload_;
    }

private:
    PackedResourceEncoding encoding_{PackedResourceEncoding::deflate};
    std::vector<std::byte> payload_;
};

}  // namespace off::data
