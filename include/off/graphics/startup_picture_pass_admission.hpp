#pragma once

#include "off/graphics/picture_view_parameters.hpp"
#include "off/graphics/startup_source_picture_draw_admission.hpp"
#include "off/runtime/startup_window_hierarchy_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

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

// One coordinator read supplies every value below. No field is derived from
// GMS directory order, a BootMenu owner, or an SDL output dimension.
struct StartupCoordinatorPassSelection {
  std::uint64_t scene_lease_identity{}, root_identity{}, camera_identity{},
      view_identity{}, pass_context_identity{}, coordinator_epoch{};
  bool camera_enabled{};
  std::array<float, 4> normalized_viewport{};
  float owner_projection_scalar{}, external_y_basis_scale{};
  PicturePassRectangle rectangle{};
};

struct StartupActivePassSnapshotServices {
  std::function<std::optional<StartupCoordinatorPassSelection>()> read_selected_pass;
  std::function<bool(std::uint64_t)> scene_lease_live;
  std::function<bool(std::uint64_t)> factory_generation_live;
  std::function<std::uint64_t()> hierarchy_epoch;
  std::function<std::uint64_t()> coordinator_epoch;
};

// Move-only proof of a single coordinator observation. It is neither a GPU
// submission nor a visible-menu claim.
class StartupActivePassSnapshot final {
public:
  StartupActivePassSnapshot(const StartupActivePassSnapshot&) = delete;
  StartupActivePassSnapshot& operator=(const StartupActivePassSnapshot&) = delete;
  StartupActivePassSnapshot(StartupActivePassSnapshot&&) noexcept = default;
  StartupActivePassSnapshot& operator=(StartupActivePassSnapshot&&) noexcept = default;
  [[nodiscard]] bool valid() const noexcept {
    return lifetime_ && selection_.scene_lease_identity != 0U &&
           selection_.root_identity != 0U && selection_.camera_identity != 0U &&
           selection_.view_identity != 0U && selection_.pass_context_identity != 0U &&
           selection_.coordinator_epoch != 0U;
  }
  [[nodiscard]] const StartupCoordinatorPassSelection& selection() const noexcept {
    return selection_;
  }
  [[nodiscard]] bool bound_to(std::uint64_t factory_generation,
                              std::uint64_t hierarchy_epoch,
                              std::uint64_t coordinator_epoch) const noexcept {
    return valid() && factory_generation_ == factory_generation &&
           hierarchy_epoch_ == hierarchy_epoch && selection_.coordinator_epoch == coordinator_epoch;
  }
private:
  friend class StartupActivePassSnapshotProvider;
  StartupActivePassSnapshot(std::shared_ptr<const void> lifetime,
                            std::uint64_t factory_generation,
                            std::uint64_t hierarchy_epoch,
                            StartupCoordinatorPassSelection selection)
      : lifetime_(std::move(lifetime)), factory_generation_(factory_generation),
        hierarchy_epoch_(hierarchy_epoch), selection_(selection) {}
  std::shared_ptr<const void> lifetime_;
  std::uint64_t factory_generation_{}, hierarchy_epoch_{};
  StartupCoordinatorPassSelection selection_{};
};

class StartupActivePassSnapshotProvider final {
public:
  [[nodiscard]] StartupActivePassSnapshot capture(
      std::shared_ptr<const void> scene_lifetime,
      const runtime::StartupWindowHierarchySnapshot& hierarchy,
      const StartupActivePassSnapshotServices& services) const {
    if (!scene_lifetime || !hierarchy.valid() || !services.read_selected_pass ||
        !services.scene_lease_live || !services.factory_generation_live ||
        !services.hierarchy_epoch || !services.coordinator_epoch)
      throw std::runtime_error("startup active pass requires live services");
    const auto selected = services.read_selected_pass();
    if (!selected || selected->scene_lease_identity == 0U || selected->root_identity == 0U ||
        selected->camera_identity == 0U || selected->view_identity == 0U ||
        selected->pass_context_identity == 0U || selected->coordinator_epoch == 0U ||
        !selected->camera_enabled || !services.scene_lease_live(selected->scene_lease_identity) ||
        !services.factory_generation_live(hierarchy.factory_generation()) ||
        !hierarchy.bound_to(hierarchy.factory_generation(), services.hierarchy_epoch()) ||
        selected->coordinator_epoch != services.coordinator_epoch())
      throw std::runtime_error("startup active pass selection is stale");
    bool root_in_hierarchy = false;
    for (const auto& node : hierarchy.nodes()) root_in_hierarchy = root_in_hierarchy || node.identity == selected->root_identity;
    if (!root_in_hierarchy) throw std::runtime_error("startup active pass root is outside live hierarchy");
    return StartupActivePassSnapshot(std::move(scene_lifetime), hierarchy.factory_generation(),
                                     hierarchy.hierarchy_epoch(), *selected);
  }
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

  [[nodiscard]] StartupPicturePassResult submit(
      const StartupActivePassSnapshot& active_pass,
      const StartupSourcePictureRuntimeSnapshot& source,
      const StartupGraphicsAsset& asset, std::uint8_t requested_state,
      const StartupSourcePictureBackendHooks& backend) {
    if (!active_pass.valid()) throw std::runtime_error("startup picture pass snapshot is invalid");
    const auto& selected = active_pass.selection();
    return submit({selected.scene_lease_identity},
                  {selected.scene_lease_identity, selected.root_identity},
                  {selected.scene_lease_identity, selected.camera_identity, selected.view_identity,
                   selected.camera_enabled, selected.normalized_viewport, selected.owner_projection_scalar},
                  {selected.scene_lease_identity, selected.pass_context_identity, selected.rectangle},
                  source, selected.external_y_basis_scale, asset, requested_state, backend);
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
