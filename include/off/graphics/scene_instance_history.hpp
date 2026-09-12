#pragma once

#include "off/graphics/scene_render.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

// Renderer-neutral snapshot of the two authored transform records carried by a
// scene instance.  They deliberately remain separate: composing the map and
// source records into one world transform is not established by the diagnostic
// scene loader.
struct SceneInstanceTransformSnapshot {
  std::array<float, 9> source_basis{};
  std::array<float, 3> source_position{};
  std::array<float, 9> map_orientation{};
  std::array<float, 3> map_position{};
  [[nodiscard]] bool operator==(const SceneInstanceTransformSnapshot &) const = default;
};

struct SceneInstanceSubmissionTransform {
  std::uint64_t identity{};
  SceneInstanceTransformSnapshot current{};
  SceneInstanceTransformSnapshot previous{};
  bool previous_valid{};
  [[nodiscard]] bool operator==(const SceneInstanceSubmissionTransform &) const = default;
};

// Builds stable identities from a single SceneRenderAsset's canonical instance
// order.  The caller must retain that asset identity for the lifecycle; this
// helper does not make an archive path, object name, or retail identifier part
// of the GPU contract.
[[nodiscard]] std::vector<SceneInstanceSubmissionTransform>
make_initial_scene_instance_submission(std::span<const SceneRenderInstance> instances);

// Keeps previous scene-instance transforms transactionally.  A future renderer
// can consume current/previous snapshots to produce motion vectors, but this
// type creates neither vectors nor GPU resources and cannot enable temporal or
// vendor upscaling.  History advances only after a real frame submission has
// succeeded; cancellation and invalidation never publish pending state.
class SceneInstanceHistoryLifecycle final {
public:
  [[nodiscard]] bool initialize(
      std::span<const SceneInstanceSubmissionTransform> instances);
  [[nodiscard]] std::optional<std::vector<SceneInstanceSubmissionTransform>>
  begin_submission(std::span<const SceneInstanceSubmissionTransform> instances);
  [[nodiscard]] bool commit_submission() noexcept;
  void cancel_submission() noexcept;
  void invalidate() noexcept;

  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] bool submission_in_flight() const noexcept { return in_flight_; }

private:
  std::map<std::uint64_t, SceneInstanceTransformSnapshot> committed_;
  std::vector<SceneInstanceSubmissionTransform> pending_;
  bool initialized_{};
  bool history_valid_{};
  bool in_flight_{};
};

} // namespace off::graphics
