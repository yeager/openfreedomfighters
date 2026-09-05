#pragma once

#include "off/data/picture_texture_binding.hpp"
#include "off/graphics/startup_graphics_asset.hpp"

#include <cstddef>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace off::graphics {

inline constexpr std::size_t maximum_startup_prepared_pictures = 21;
inline constexpr std::size_t maximum_startup_prepared_submissions = 77;

struct StartupGraphicsPreparedResource {
  std::size_t resource_index{};
  std::size_t catalog_image_index{};
  std::uint32_t texture_id{};
  std::uint32_t width{};
  std::uint32_t height{};
};

struct StartupGraphicsPreparedPicture {
  std::size_t row_index{};
  std::size_t picture_index{};
  std::size_t row_directory_index{};
  std::size_t picture_directory_index{};
  data::StartupGraphicsCompositionRole role{};
  std::uint32_t base_render_property{};
  std::uint8_t authored_alpha{255};
  std::uint8_t alignment_enum{};
  std::optional<std::uint8_t> extension_control;
  std::size_t first_submission{};
  std::size_t submission_count{};
};

// A raw descriptor-backed quad. UV extrema remain unpaired with corners; this
// record establishes neither topology, winding, blend nor material behavior.
struct StartupGraphicsPreparedQuad {
  std::size_t emission_ordinal{};
  std::size_t row_index{};
  std::size_t picture_index{};
  std::size_t picture_directory_index{};
  std::size_t group_index{};
  std::size_t descriptor_index{};
  std::size_t resource_index{};
  data::PictureQuad source;
  // Initial authored per-group provenance, not later mutable runtime material.
  // Records sharing an image may still differ.
  std::uint32_t texture_resource_prm_offset{};
  std::array<std::byte, 32> authored_texture_resource_record{};
};

struct StartupGraphicsPreparedSubmission {
  std::size_t emission_ordinal{};
  std::size_t prepared_picture_index{};
  std::size_t prepared_quad_index{};
};

class StartupGraphicsPreparedPlan final {
public:
  StartupGraphicsPreparedPlan(StartupGraphicsPreparedPlan &&) noexcept = default;
  StartupGraphicsPreparedPlan &
  operator=(StartupGraphicsPreparedPlan &&) noexcept = default;
  StartupGraphicsPreparedPlan(const StartupGraphicsPreparedPlan &) = delete;
  StartupGraphicsPreparedPlan &
  operator=(const StartupGraphicsPreparedPlan &) = delete;

  [[nodiscard]] std::uint8_t requested_state() const noexcept {
    return requested_state_;
  }
  [[nodiscard]] std::uint8_t effective_state() const noexcept {
    return effective_state_;
  }
  [[nodiscard]] const std::vector<StartupGraphicsPreparedResource> &
  resources() const noexcept { return resources_; }
  [[nodiscard]] const std::vector<StartupGraphicsPreparedPicture> &
  pictures() const noexcept { return pictures_; }
  [[nodiscard]] const std::vector<StartupGraphicsPreparedQuad> &
  quads() const noexcept { return quads_; }
  [[nodiscard]] const std::vector<StartupGraphicsPreparedSubmission> &
  submissions() const noexcept { return submissions_; }

private:
  StartupGraphicsPreparedPlan() = default;
  friend StartupGraphicsPreparedPlan
  prepare_startup_graphics_plan(const StartupGraphicsAsset &, std::uint8_t);

  std::uint8_t requested_state_{};
  std::uint8_t effective_state_{};
  std::vector<StartupGraphicsPreparedResource> resources_;
  std::vector<StartupGraphicsPreparedPicture> pictures_;
  std::vector<StartupGraphicsPreparedQuad> quads_;
  std::vector<StartupGraphicsPreparedSubmission> submissions_;
};

// Preserves the recovered CPU traversal and immediate emission sequence. It is
// deliberately not a GPU plan and contains no rasterization decisions.
[[nodiscard]] StartupGraphicsPreparedPlan
prepare_startup_graphics_plan(const StartupGraphicsAsset &asset,
                              std::uint8_t requested_state);

} // namespace off::graphics
