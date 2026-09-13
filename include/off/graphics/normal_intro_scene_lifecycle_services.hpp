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

// Concrete operations for a retained normal-scene session. The session, not
// this caller-supplied table, owns the named/global bytes, renderer bytes, and
// List-association pairs used by the outer tail: binding replaces those three
// input fields with its parser-validated source receipts. Callers supply only
// the concrete allocation-state, association, saved-resource and other
// operation boundaries. This type does not provide a lifecycle fallback.
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
  // Binds only the continuation boundaries for a retained session that has
  // already recorded its ordinary reader bracket. The returned reader callback
  // is intentionally empty and therefore cannot be passed to normal activate.
  [[nodiscard]] static NormalIntroSceneHostLifecycleServices bind_after_reader_bracket(
      std::shared_ptr<NormalIntroSceneSession> session,
      NormalIntroSceneLifecycleAdapterConfig config);
};

}  // namespace off::graphics
