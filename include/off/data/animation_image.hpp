#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace off::data {

// The ANM payload is retained as an opaque animation resource for now. This
// header is the complete, independently verified envelope shared by the
// supported installation; tracks and bindings remain unrecovered.
struct AnimationImageHeader {
  std::size_t byte_size{};
  std::uint32_t major_version{};
  std::uint32_t minor_version{};
};

class AnimationImage final {
public:
  [[nodiscard]] static AnimationImage parse(std::span<const std::byte> bytes);

  [[nodiscard]] const AnimationImageHeader &header() const noexcept {
    return header_;
  }

private:
  AnimationImageHeader header_{};
};

} // namespace off::data
