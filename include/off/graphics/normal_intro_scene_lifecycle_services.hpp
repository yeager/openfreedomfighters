#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/normal_intro_scene_session.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace off::graphics {

// The three startup boundaries remain independently owned. A host factory
// accepts this shape but never invokes it while constructing the host.
struct NormalIntroSceneHostLifecycleServices {
  std::function<void()> reader_bracket;
  std::function<void()> outer_loader_tail;
  std::function<void()> enter_global_lifecycle;
};

// Concrete services for a retained normal-scene session. They intentionally
// contain no replacement parser, fabricated owner map, or default lifecycle
// callback. The caller supplies the real outer-tail and global-lifecycle work.
struct NormalIntroSceneLifecycleAdapterConfig {
  std::uint64_t retained_saved_value{};
  IntroOuterLoaderTailServices outer_loader_tail;
  std::function<void(NormalIntroSceneSession&)> enter_global_lifecycle;
};

// Produces host-facing callbacks from an owned normal-scene session. Each
// callback validates the preceding session receipt before doing work and holds
// the session strongly, so a host cannot retain a callback into a destroyed
// loader. Binding this object alone has no startup side effects.
class NormalIntroSceneLifecycleServiceAdapters final {
 public:
  [[nodiscard]] static NormalIntroSceneHostLifecycleServices bind(
      std::shared_ptr<NormalIntroSceneSession> session,
      NormalIntroSceneLifecycleAdapterConfig config);
};

}  // namespace off::graphics
