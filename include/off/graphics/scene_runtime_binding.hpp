#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/scene_render.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace off::graphics {

// Binds each canonical diagnostic scene instance to its constructed runtime
// owner. This proves identity/resource association only; it does not produce a
// world transform, camera state, visibility decision, or draw submission.
struct SceneRuntimeInstanceBinding {
  std::size_t scene_instance_index{};
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
