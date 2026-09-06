#pragma once
#include <array>

namespace off::graphics {
// Portable native extended-precision policy: exact binary32 products, ordered
// sums rounded to 64 significant bits, then binary32 stores (nearest/even).
// The original live x87 precision-control setting is not established. This is
// explicit interoperability, not a universal bit-exact original-runtime claim.
[[nodiscard]] std::array<float,3> transform_preview_translation(
    const std::array<float,3>& scaled_local,const std::array<float,9>& basis);
} // namespace off::graphics
