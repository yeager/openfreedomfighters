#pragma once

#include "off/runtime/startup_window_hierarchy_snapshot.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace off::runtime {

// A scene-manager supplied selection, rather than a directory index or a
// guessed BootMenu/graphics node. The pass-context identity remains opaque to
// the hierarchy code and is consumed only by a later renderer boundary.
struct StartupManagerSelectedRoot {
  std::uint64_t scene_lease_identity{};
  std::uint64_t root_identity{};
  std::uint64_t pass_context_identity{};
};

struct StartupActiveWindowRootServices {
  // Returns the coordinator's already-selected live root. This boundary never
  // chooses a root from parsed source data or a hierarchy traversal.
  std::function<std::optional<StartupManagerSelectedRoot>()> selected_root;
  std::function<bool(std::uint64_t)> scene_lease_live;
  std::function<bool(std::uint64_t)> factory_generation_live;
  std::function<std::uint64_t()> hierarchy_epoch;
};

// Move-only proof that one manager-selected root still agrees with a complete
// factory-built hierarchy snapshot. It is not an admission to render: a later
// camera/view and renderer-state pass must still be independently proven.
class StartupActiveWindowRootToken final {
public:
  StartupActiveWindowRootToken(const StartupActiveWindowRootToken&) = delete;
  StartupActiveWindowRootToken& operator=(const StartupActiveWindowRootToken&) = delete;
  StartupActiveWindowRootToken(StartupActiveWindowRootToken&&) noexcept = default;
  StartupActiveWindowRootToken& operator=(StartupActiveWindowRootToken&&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return lifetime_ && selection_.scene_lease_identity != 0U &&
           selection_.root_identity != 0U &&
           selection_.pass_context_identity != 0U;
  }
  [[nodiscard]] const StartupManagerSelectedRoot& selection() const noexcept {
    return selection_;
  }

private:
  friend class StartupActiveWindowRootProvider;
  StartupActiveWindowRootToken(std::shared_ptr<const void> lifetime,
                               StartupManagerSelectedRoot selection)
      : lifetime_(std::move(lifetime)), selection_(selection) {}

  std::shared_ptr<const void> lifetime_;
  StartupManagerSelectedRoot selection_{};
};

class StartupActiveWindowRootProvider final {
public:
  [[nodiscard]] StartupActiveWindowRootToken admit(
      std::shared_ptr<const void> scene_lifetime,
      const StartupWindowHierarchySnapshot& hierarchy,
      const StartupActiveWindowRootServices& services) const {
    if (!scene_lifetime || !hierarchy.valid() || !services.selected_root ||
        !services.scene_lease_live || !services.factory_generation_live ||
        !services.hierarchy_epoch)
      throw std::runtime_error("startup active root requires live services");
    const auto selected = services.selected_root();
    if (!selected || selected->scene_lease_identity == 0U ||
        selected->root_identity == 0U || selected->pass_context_identity == 0U ||
        !services.scene_lease_live(selected->scene_lease_identity) ||
        !services.factory_generation_live(hierarchy.factory_generation()) ||
        !hierarchy.bound_to(hierarchy.factory_generation(), services.hierarchy_epoch()))
      throw std::runtime_error("startup active root selection is stale");
    bool found = false;
    for (const auto& node : hierarchy.nodes())
      found = found || node.identity == selected->root_identity;
    if (!found)
      throw std::runtime_error("startup active root is outside the live hierarchy");
    return StartupActiveWindowRootToken(std::move(scene_lifetime), *selected);
  }
};

}  // namespace off::runtime
