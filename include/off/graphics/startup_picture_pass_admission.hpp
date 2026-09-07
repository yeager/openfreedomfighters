#pragma once

#include "off/graphics/picture_view_parameters.hpp"
#include "off/graphics/startup_source_picture_draw_admission.hpp"

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::graphics {

// These identities are issued by live services. They are deliberately separate
// from parsed directory indices and diagnostic UI identifiers.
struct StartupSceneLease {
  std::uint64_t identity{};
};

struct StartupActiveWindowTraversalRoot {
  std::uint64_t scene_lease_identity{};
  std::uint64_t root_identity{};
};

struct StartupAdmittedCameraView {
  std::uint64_t scene_lease_identity{};
  std::uint64_t camera_identity{};
  std::uint64_t view_identity{};
  bool enabled{};
  // Current retained camera viewport tuple. Its final two elements are spans.
  std::array<float, 4> normalized_viewport{};
  float owner_projection_scalar{};
};

struct StartupRendererStatePassContext {
  std::uint64_t scene_lease_identity{};
  std::uint64_t identity{};
  // This is the renderer state's retained rectangle, not a host drawable size.
  PicturePassRectangle rectangle{};
};

struct StartupSourcePictureRuntimeSnapshot {
  std::uint64_t scene_lease_identity{};
  StartupSourcePictureTraversalSnapshot traversal{};
  std::span<const StartupGraphicsPictureTransform> transforms;
};

struct StartupPicturePassResult {
  std::size_t prepared_picture_count{};
  std::size_t submitted_group_count{};
};

// Strictly admits a previously selected live window root and camera/view into
// the existing source-picture record/draw boundary. It neither selects the
// root/camera nor constructs a state rectangle or a startup menu.
class StartupPicturePassAdmission final {
public:
  StartupPicturePassAdmission() = default;
  StartupPicturePassAdmission(const StartupPicturePassAdmission&) = delete;
  StartupPicturePassAdmission& operator=(const StartupPicturePassAdmission&) = delete;
  StartupPicturePassAdmission(StartupPicturePassAdmission&&) = delete;
  StartupPicturePassAdmission& operator=(StartupPicturePassAdmission&&) = delete;

  [[nodiscard]] StartupPicturePassResult submit(
      const StartupSceneLease& scene, const StartupActiveWindowTraversalRoot& root,
      const StartupAdmittedCameraView& camera_view,
      const StartupRendererStatePassContext& pass,
      const StartupSourcePictureRuntimeSnapshot& source,
      float external_y_basis_scale, const StartupGraphicsAsset& asset,
      std::uint8_t requested_state,
      const StartupSourcePictureBackendHooks& backend) {
    validate(scene, root, camera_view, pass, source, external_y_basis_scale);
    const auto result = downstream_.submit(asset, requested_state,
                                           source.traversal, source.transforms,
                                           backend);
    return {result.prepared_picture_count, result.submitted_group_count};
  }

private:
  static void validate(const StartupSceneLease& scene,
                       const StartupActiveWindowTraversalRoot& root,
                       const StartupAdmittedCameraView& camera_view,
                       const StartupRendererStatePassContext& pass,
                       const StartupSourcePictureRuntimeSnapshot& source,
                       float external_y_basis_scale) {
    if (scene.identity == 0U || root.scene_lease_identity != scene.identity ||
        camera_view.scene_lease_identity != scene.identity ||
        pass.scene_lease_identity != scene.identity ||
        source.scene_lease_identity != scene.identity)
      throw std::runtime_error("startup picture pass scene lease is inconsistent");
    if (root.root_identity == 0U ||
        source.traversal.root_identity != root.root_identity)
      throw std::runtime_error("startup picture pass active root is inconsistent");
    if (pass.identity == 0U ||
        source.traversal.pass_context_identity != pass.identity)
      throw std::runtime_error("startup picture pass renderer context is inconsistent");
    if (camera_view.camera_identity == 0U || camera_view.view_identity == 0U ||
        !camera_view.enabled)
      throw std::runtime_error("startup picture pass camera view is not admitted");
    if (!finite_positive_extent(pass.rectangle) ||
        !finite_camera_viewport(camera_view.normalized_viewport) ||
        !std::isfinite(camera_view.owner_projection_scalar) ||
        !std::isfinite(external_y_basis_scale))
      throw std::runtime_error("startup picture pass inputs are invalid");
    for (const auto& association : source.traversal.owner_views)
      if (association.admitted_view_identity != camera_view.view_identity)
        throw std::runtime_error("startup picture pass owner/view association is inconsistent");
  }

  [[nodiscard]] static bool finite_positive_extent(const PicturePassRectangle& rectangle) {
    return std::isfinite(rectangle.left) && std::isfinite(rectangle.top) &&
        std::isfinite(rectangle.right) && std::isfinite(rectangle.bottom) &&
        rectangle.right > rectangle.left && rectangle.bottom > rectangle.top;
  }

  [[nodiscard]] static bool finite_camera_viewport(
      const std::array<float, 4>& viewport) {
    for (const auto value : viewport)
      if (!std::isfinite(value)) return false;
    // Native safety policy for the restricted pass: nonnegative origin and a
    // positive in-range span. It is not asserted as a retail clamp.
    return viewport[0] >= 0.0F && viewport[1] >= 0.0F &&
        viewport[2] > 0.0F && viewport[3] > 0.0F &&
        viewport[2] <= 1.0F && viewport[3] <= 1.0F;
  }

  StartupSourcePictureDrawAdmission downstream_;
};

}  // namespace off::graphics
