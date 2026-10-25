#pragma once

#include "off/data/startup_graphics_composition.hpp"
#include "off/data/texture_catalog.hpp"
#include "off/graphics/texture_decode.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace off::graphics {

inline constexpr std::size_t maximum_startup_graphics_images = 4096;

// Project-authored resource limits for decoded CPU images and uploaded bytes.
// It bounds the decoded CPU image set that will later cross the GPU boundary.
inline constexpr std::size_t startup_graphics_decoded_byte_budget =
    64U * 1024U * 1024U;

struct StartupGraphicsImage {
  // Identity is local to the paired TEX catalog used to build this asset.
  std::size_t catalog_image_index{};
  std::uint32_t texture_id{};
  RgbaImage mip_zero;
};

class StartupGraphicsAsset final {
public:
  StartupGraphicsAsset(StartupGraphicsAsset &&) noexcept = default;
  StartupGraphicsAsset &operator=(StartupGraphicsAsset &&) noexcept = default;
  StartupGraphicsAsset(const StartupGraphicsAsset &) = delete;
  StartupGraphicsAsset &operator=(const StartupGraphicsAsset &) = delete;

  [[nodiscard]] const data::StartupGraphicsComposition &
  composition() const noexcept {
    return composition_;
  }
  [[nodiscard]] const std::vector<StartupGraphicsImage> &
  images() const noexcept {
    return images_;
  }

private:
  StartupGraphicsAsset() = default;
  friend StartupGraphicsAsset build_startup_graphics_asset(
      data::StartupGraphicsComposition, const data::TextureCatalog &,
      std::size_t);
  data::StartupGraphicsComposition composition_;
  std::vector<StartupGraphicsImage> images_;
};

// Builds an owning image set from an already provenance-paired composition and
// TEX catalog. The budget parameter exists for deterministic policy tests.
[[nodiscard]] StartupGraphicsAsset build_startup_graphics_asset(
    data::StartupGraphicsComposition composition,
    const data::TextureCatalog &textures,
    std::size_t decoded_byte_budget = startup_graphics_decoded_byte_budget);

// Precondition: install verification has accepted the local startup archive.
// The loader still revalidates its unique paired member structure.
[[nodiscard]] StartupGraphicsAsset
load_startup_graphics_asset(const std::filesystem::path &startup_archive);

} // namespace off::graphics
