#include "off/graphics/intro_outer_loader_tail_readiness.hpp"

namespace off::graphics {

IntroOuterLoaderTailReadiness inspect_intro_outer_loader_tail_readiness(
    const data::GmsOuterLoaderSources &sources) {
  IntroOuterLoaderTailReadiness result{
      .named_global_bytes = sources.named_global ? sources.named_global->size() : 0U,
      .renderer_resource_bytes =
          sources.renderer_resource ? sources.renderer_resource->size() : 0U,
      .resource_association_count = sources.resource_associations.size(),
      .allocation_sizing_row_count = sources.allocation_sizing_rows.size(),
      .required_boundaries = {}};
  if (sources.named_global)
    result.required_boundaries.push_back(
        IntroOuterLoaderTailBoundary::named_global_relocation_and_reader);
  if (sources.renderer_resource)
    result.required_boundaries.push_back(
        IntroOuterLoaderTailBoundary::renderer_reference_resolution_and_container_parser);
  if (!sources.resource_associations.empty())
    result.required_boundaries.push_back(
        IntroOuterLoaderTailBoundary::live_resource_association);
  result.required_boundaries.insert(result.required_boundaries.end(), {
      IntroOuterLoaderTailBoundary::loader_source_lease_release,
      IntroOuterLoaderTailBoundary::camera_zero_query_and_fallback_registration,
      IntroOuterLoaderTailBoundary::outer_scene_operations,
      IntroOuterLoaderTailBoundary::saved_resource_spatial_admission,
      IntroOuterLoaderTailBoundary::saved_resource_0x4000_service});
  return result;
}

const char *intro_outer_loader_tail_boundary_label(
    IntroOuterLoaderTailBoundary boundary) noexcept {
  switch (boundary) {
  case IntroOuterLoaderTailBoundary::named_global_relocation_and_reader:
    return "named-global-relocation-and-reader";
  case IntroOuterLoaderTailBoundary::renderer_reference_resolution_and_container_parser:
    return "renderer-reference-resolution-and-container-parser";
  case IntroOuterLoaderTailBoundary::live_resource_association:
    return "live-resource-association";
  case IntroOuterLoaderTailBoundary::loader_source_lease_release:
    return "loader-source-lease-release";
  case IntroOuterLoaderTailBoundary::camera_zero_query_and_fallback_registration:
    return "camera-zero-query-and-fallback-registration";
  case IntroOuterLoaderTailBoundary::outer_scene_operations:
    return "outer-scene-operations";
  case IntroOuterLoaderTailBoundary::saved_resource_spatial_admission:
    return "saved-resource-spatial-admission";
  case IntroOuterLoaderTailBoundary::saved_resource_0x4000_service:
    return "saved-resource-0x4000-service";
  }
  return "unknown";
}

} // namespace off::graphics
