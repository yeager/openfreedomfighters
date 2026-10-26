#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/scene_render.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

// Binds each canonical diagnostic scene instance to both the source selected
// by its map handle and the source providing its primitive. These identities
// may differ for expanded group descendants. The binding proves parsed
// map-to-GMS-slot identity and runtime resource association only; it does not
// establish original renderer selection, world transforms, camera state,
// visibility, or draw submission.
struct SceneRuntimeInstanceBinding {
  std::size_t scene_instance_index{};
  SceneRenderMapKind map_kind{SceneRenderMapKind::rmc};
  std::size_t map_layer_index{};
  std::size_t map_entry_index{};
  SceneGeometryRole geometry_role{SceneGeometryRole::primary};
  std::uint32_t map_descriptor_offset{};
  std::uint32_t geometry_reference{};
  std::optional<std::size_t> map_source_directory_index;
  std::optional<IntroRuntimeHandle> map_owner;
  std::size_t source_directory_index{};
  IntroRuntimeHandle owner{};
  IntroRuntimeResourceHandle resource{};
  std::optional<IntroRuntimeHandle> parent_owner;
};

// The asset must have been built from the same authored GMS directory used to
// construct runtime. The source-pose comparison catches mismatched rows but is
// not a substitute for that provenance requirement. Call after directory
// construction and before gameplay updates mutate source poses.
[[nodiscard]] std::vector<SceneRuntimeInstanceBinding>
bind_scene_instances_to_runtime(const SceneRenderAsset &asset,
                                const IntroRuntime &runtime);

} // namespace off::graphics
