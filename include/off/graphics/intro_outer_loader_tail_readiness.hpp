#pragma once

#include "off/data/gms_image.hpp"

#include <cstddef>
#include <vector>

namespace off::graphics {

// Source-backed preflight for the ordinary loader tail. This reports the
// retained inputs and concrete boundaries still needed to run it; it neither
// creates callbacks nor advances IntroRuntime.
enum class IntroOuterLoaderTailBoundary {
  named_global_relocation_and_reader,
  renderer_allocation_diagnostic_state,
  live_resource_association,
  loader_source_lease_release,
  camera_zero_query_and_fallback_registration,
  outer_scene_operations,
  saved_resource_spatial_admission,
  saved_resource_0x4000_service,
};

struct IntroOuterLoaderTailReadiness {
  std::size_t named_global_bytes{};
  bool named_global_native_supported{};
  std::size_t renderer_resource_bytes{};
  std::size_t resource_association_count{};
  std::size_t allocation_sizing_row_count{};
  std::vector<IntroOuterLoaderTailBoundary> required_boundaries;

  [[nodiscard]] bool ready_to_run() const noexcept {
    return required_boundaries.empty();
  }
};

[[nodiscard]] IntroOuterLoaderTailReadiness
inspect_intro_outer_loader_tail_readiness(
    const data::GmsOuterLoaderSources &sources);
[[nodiscard]] const char *intro_outer_loader_tail_boundary_label(
    IntroOuterLoaderTailBoundary boundary) noexcept;

} // namespace off::graphics
