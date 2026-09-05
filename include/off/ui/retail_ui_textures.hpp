#pragma once

#include "off/graphics/texture_decode.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace off::ui {

// Semantic roles recovered from the retail UI. Numeric catalog identifiers and
// resource names deliberately do not cross this runtime boundary.
enum class RetailUiTextureRole : std::uint8_t {
  scanlines_top,
  scanlines_bottom,
  black_fill_top,
  black_fill_bottom,
  border_fragment_1,
  border_fragment_2,
  border_fragment_3,
  border_fragment_4,
  border_fragment_5,
  border_fragment_6,
  border_fragment_7,
  border_fragment_8,
  arrow_left,
  arrow_right,
  arrow_up,
  arrow_down,
};

struct RetailUiTexture {
  RetailUiTextureRole role{};
  graphics::RgbaImage mip_zero;
};

// Produced only after a recovered UI-picture resource join. The image index is
// ephemeral parser state, never a published retail identifier or persistent
// contract.
struct RetailUiTextureBinding {
  RetailUiTextureRole role{};
  std::size_t image_index{};
  auto operator<=>(const RetailUiTextureBinding &) const = default;
};

class RetailUiTextureSet final {
public:
  [[nodiscard]] std::span<const RetailUiTexture> textures() const noexcept {
    return textures_;
  }
  [[nodiscard]] const RetailUiTexture *
  find(RetailUiTextureRole role) const noexcept;

private:
  friend RetailUiTextureSet
  resolve_retail_ui_textures(std::span<const data::TextureImage> images,
                             std::span<const RetailUiTextureBinding> bindings);
  std::vector<RetailUiTexture> textures_;
};

// Decodes a complete role mapping recovered from UI-picture resource
// relationships. This function never guesses from resource names, catalog order,
// or pixels.
[[nodiscard]] RetailUiTextureSet
resolve_retail_ui_textures(std::span<const data::TextureImage> images,
                           std::span<const RetailUiTextureBinding> bindings);

// Reads the unique TEX member directly from the user's startup archive. Retail
// image bytes remain memory-owned and are never extracted to the filesystem.
[[nodiscard]] RetailUiTextureSet
load_retail_ui_textures(const std::filesystem::path &startup_archive,
                        std::span<const RetailUiTextureBinding> bindings);

} // namespace off::ui
