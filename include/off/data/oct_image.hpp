#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace off::data {

// This is only the corpus-validated fixed OCT envelope. The bounded pair is
// intentionally not named as a node, cell, object, or payload range: its
// consumer has not been recovered.
struct OctImageStructuralBounds final {
  std::size_t lower{};
  std::size_t upper{};
};

struct OctImageEnvelope final {
  std::size_t byte_size{};
  bool repeated_header_flag{};
  std::optional<OctImageStructuralBounds> structural_bounds;
};

class OctImage final {
public:
  [[nodiscard]] static OctImage parse(std::span<const std::byte> bytes);

  [[nodiscard]] const OctImageEnvelope &envelope() const noexcept {
    return envelope_;
  }

private:
  OctImageEnvelope envelope_{};
};

} // namespace off::data
