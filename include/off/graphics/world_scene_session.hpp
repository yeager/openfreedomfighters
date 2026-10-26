#pragma once

#include "off/data/gms_image.hpp"
#include "off/graphics/scene_gpu_plan.hpp"
#include "off/graphics/scene_render.hpp"
#include "off/graphics/world_scene_identity.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace off::graphics {

// Owns one checked, explicitly selected scene package and its source-derived
// geometry candidate plan. The plan remains a diagnostic SDL input: this
// session does not admit gameplay transforms, camera/view state, culling,
// visibility, or original draw order.
class WorldSceneSession final {
public:
  [[nodiscard]] static WorldSceneSession load_owned_checked(
      const std::filesystem::path &install_root,
      const std::filesystem::path &relative_archive_path);

  WorldSceneSession(WorldSceneSession &&) noexcept;
  WorldSceneSession &operator=(WorldSceneSession &&) noexcept;
  WorldSceneSession(const WorldSceneSession &) = delete;
  WorldSceneSession &operator=(const WorldSceneSession &) = delete;
  ~WorldSceneSession();

  [[nodiscard]] const SceneGpuPlan &diagnostic_plan() const noexcept;
  // These instances are the same candidates carried by the SDL plan. The
  // directory row indices in them are scoped by this retained GMS image.
  [[nodiscard]] std::span<const SceneGpuInstance>
  candidate_instances() const noexcept;
  [[nodiscard]] std::span<const data::GmsDirectoryEntry>
  source_directory() const noexcept;
  [[nodiscard]] std::span<const data::GmsHierarchyNode>
  source_hierarchy() const noexcept;
  [[nodiscard]] WorldSceneSessionId identity() const noexcept;
  [[nodiscard]] std::span<const WorldSceneSourceOwner>
  source_owners() const noexcept;
  [[nodiscard]] const SceneRenderResolutionSummary &resolution_summary()
      const noexcept;

private:
  struct Storage;
  explicit WorldSceneSession(std::unique_ptr<Storage> storage) noexcept;

  std::unique_ptr<Storage> storage_;
};

// Checks that every render candidate refers to the same parsed GMS source
// rows and authored ancestry that produced the render asset. This is a
// structural package-to-candidate check; it does not infer map transform or
// renderer selection behavior.
void validate_world_scene_asset_provenance(
    const SceneRenderAsset &asset,
    std::span<const data::GmsDirectoryEntry> source_directory,
    std::span<const data::GmsHierarchyNode> source_hierarchy);

[[nodiscard]] std::vector<WorldSceneSourceOwner>
build_world_scene_source_owner_table(
    std::span<const data::GmsDirectoryEntry> source_directory,
    std::span<const data::GmsHierarchyNode> source_hierarchy,
    WorldSceneSessionId session_id);

void bind_world_scene_candidate_owners(
    SceneGpuPlan &plan,
    std::span<const WorldSceneSourceOwner> source_owners,
    WorldSceneSessionId session_id);

} // namespace off::graphics
