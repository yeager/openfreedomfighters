#include "off/data/gms_image.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace off::data {
namespace {

constexpr std::uint32_t reference_flag_mask = 0xc0000000U;
constexpr std::uint32_t gms_reference_flag = 0x40000000U;
constexpr std::uint32_t reference_offset_mask = 0x3fffffffU;

}  // namespace

GmsImage GmsImage::parse(PackedResource resource) {
    GmsImage result;
    result.resource_ = std::move(resource);
    return result;
}

std::span<const std::byte> GmsImage::resolve_reference(
    std::uint32_t packed_reference,
    std::size_t size
) const {
    if ((packed_reference & reference_flag_mask) != gms_reference_flag) {
        throw std::runtime_error("invalid GMS reference tag");
    }
    const auto offset = static_cast<std::size_t>(
        packed_reference & reference_offset_mask
    );
    const auto payload = resource_.payload();
    if (size == 0 || offset > payload.size() || size > payload.size() - offset) {
        throw std::runtime_error("GMS reference exceeds decoded image bounds");
    }
    return payload.subspan(offset, size);
}

}  // namespace off::data
