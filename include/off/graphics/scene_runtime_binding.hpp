#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/scene_render.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

// Binds each canonical diagnostic scene instance to its map identity and
// constructed runtime owner. This proves identity/resource association and
// authored parent ancestry only; it does not produce a world transform,
// camera state, visibility decision, or draw submission.
struct SceneRuntimeInstanceBinding {
  std::size_t scene_instance_index{};
  SceneRenderMapKind map_kind{SceneRenderMapKind::rmc};
  std::size_t map_layer_index{};
  std::size_t map_entry_index{};
  SceneGeometryRole geometry_role{SceneGeometryRole::primary};
  std::uint32_t map_descriptor_offset{};
  std::uint32_t geometry_reference{};
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
