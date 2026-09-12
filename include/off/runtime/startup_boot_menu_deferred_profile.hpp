#pragma once

#include "off/data/deferred_compact_block_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::runtime {

// Payload-free framing evidence for the checked BootMenu deferred block. The
// four-byte GMS block header is validated before the generic profiler sees its
// body. This exposes no decoded scalar, string, attachment, component, or
// reader behavior.
struct StartupBootMenuDeferredProfile final {
  std::size_t body_bytes{};
  data::DeferredCompactBlockProfile framing;

  [[nodiscard]] bool operator==(
      const StartupBootMenuDeferredProfile &) const = default;
};

class StartupBootMenuDeferredProfiler final {
public:
  [[nodiscard]] static StartupBootMenuDeferredProfile profile(
      std::span<const std::byte> header_inclusive_block) {
    constexpr std::size_t header_size = sizeof(std::uint32_t);
    if (header_inclusive_block.size() <= header_size) {
      throw std::runtime_error(
          "startup BootMenu deferred block has no body to profile");
    }

    std::uint32_t header{};
    for (std::size_t index = 0; index < header_size; ++index) {
      header |= static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(header_inclusive_block[index]))
                << static_cast<unsigned>(index * 8U);
    }
    constexpr std::uint32_t size_mask = 0x00ffffffU;
    if ((header >> 24U) != 0U ||
        static_cast<std::size_t>(header & size_mask) !=
            header_inclusive_block.size()) {
      throw std::runtime_error(
          "startup BootMenu deferred block header is not validated");
    }

    const auto body = header_inclusive_block.subspan(header_size);
    return {.body_bytes = body.size(),
            .framing = data::DeferredCompactBlockProfiler::profile(body)};
  }
};

} // namespace off::runtime
