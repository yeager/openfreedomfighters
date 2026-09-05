#pragma once

#include "off/graphics/picture_expansion.hpp"
#include "off/graphics/startup_graphics_prepared_plan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::graphics {

struct StartupGraphicsPictureTransform {
  std::size_t picture_directory_index{};
  PictureCacheTransform transform{};
};

struct StartupGraphicsExpandedSubmission {
  std::size_t emission_ordinal{};
  std::size_t prepared_picture_index{};
  std::size_t picture_directory_index{};
  std::size_t row_index{};
  std::size_t picture_index{};
  std::size_t group_index{};
  std::size_t descriptor_index{};
  std::size_t resource_index{};
  std::array<ExpandedPictureVertex, 4> vertices{};
  // Local to this submission's four vertices. No resource-crossing batching.
  std::array<std::uint16_t, 6> indices{};
};

class StartupGraphicsExpandedPlan final {
public:
  [[nodiscard]] std::uint8_t requested_state() const noexcept {
    return requested_state_;
  }
  [[nodiscard]] std::uint8_t effective_state() const noexcept {
    return effective_state_;
  }
  [[nodiscard]] const std::vector<StartupGraphicsPreparedResource> &
  resources() const noexcept {
    return resources_;
  }
  [[nodiscard]] const std::vector<StartupGraphicsPreparedPicture> &
  pictures() const noexcept {
    return pictures_;
  }
  [[nodiscard]] const std::vector<StartupGraphicsExpandedSubmission> &
  submissions() const noexcept {
    return submissions_;
  }

private:
  StartupGraphicsExpandedPlan() = default;
  friend StartupGraphicsExpandedPlan expand_startup_graphics_plan(
      const StartupGraphicsPreparedPlan &,
      std::span<const StartupGraphicsPictureTransform>);
  std::uint8_t requested_state_{};
  std::uint8_t effective_state_{};
  std::vector<StartupGraphicsPreparedResource> resources_;
  std::vector<StartupGraphicsPreparedPicture> pictures_;
  std::vector<StartupGraphicsExpandedSubmission> submissions_;
};

// Owns metadata and conditional CPU geometry, never pixels or GPU state.
// Requires exactly one explicit transform per prepared picture directory
// identity, in any order. Missing, duplicate, extra, or non-finite transforms
// throw std::runtime_error; no transform or runtime renderer policy is
// inferred.
[[nodiscard]] StartupGraphicsExpandedPlan expand_startup_graphics_plan(
    const StartupGraphicsPreparedPlan &prepared,
    std::span<const StartupGraphicsPictureTransform> transforms);

} // namespace off::graphics
