#pragma once

#include "off/runtime/startup_scene_loader.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace off::runtime {

class StartupBootMenuAdmission;
class StartupBootSceneFactory;

// Evidence supplied by the completed FF-StartUp directory reader. This is not
// a parser: callers must have checked the entire mapped directory first.
struct StartupBootSceneDirectoryProof {
  bool complete_directory_mapping{};
  bool canonical_ordinary_window_source{};
  std::string_view component_identifier;
  float component_parameter{};
};

// The caller owns this lease for the full scene transaction. It is separate
// from archive parsing and keeps the constructed runtime scene alive.
class StartupBootSceneLease final {
public:
  [[nodiscard]] static StartupBootSceneLease
  live(std::shared_ptr<const void> lifetime) {
    if (!lifetime)
      throw std::runtime_error("Startup boot scene has no lifetime");
    return StartupBootSceneLease(std::move(lifetime));
  }

private:
  friend class StartupBootSceneConstruction;
  explicit StartupBootSceneLease(std::shared_ptr<const void> lifetime)
      : lifetime_(std::move(lifetime)) {}
  std::shared_ptr<const void> lifetime_;
};

struct StartupBootSceneConstructionServices {
  // Class registry and allocation are deliberately distinct. Neither source
  // labels nor diagnostic UI can manufacture either returned identity.
  std::function<bool()> registry_live;
  std::function<std::uint64_t()> allocate_ordinary_window;
  std::function<bool(std::uint64_t)> canonical_live_window_owner;
  std::function<std::uint64_t(std::uint64_t, float)> attach_boot_menu_component;
  std::function<bool(std::uint64_t)> live_boot_menu_component;
};

// Move-only proof that the checked source-backed BootMenu component exists in
// one live scene transaction. It is not initialized and has no input, focus,
// render, or scene-selection behavior.
class StartupBootControllerToken final {
public:
  StartupBootControllerToken(const StartupBootControllerToken &) = delete;
  StartupBootControllerToken &
  operator=(const StartupBootControllerToken &) = delete;
  StartupBootControllerToken(StartupBootControllerToken &&) noexcept = default;
  StartupBootControllerToken &
  operator=(StartupBootControllerToken &&) noexcept = default;

  [[nodiscard]] std::uint64_t owner() const noexcept { return owner_; }
  [[nodiscard]] std::uint64_t component() const noexcept { return component_; }
  [[nodiscard]] std::uint64_t factory_generation() const noexcept {
    return factory_generation_;
  }
  [[nodiscard]] bool valid() const noexcept {
    return package_ && scene_lifetime_ && owner_ != 0U && component_ != 0U &&
           factory_generation_ != 0U;
  }

private:
  friend class StartupBootSceneConstruction;
  friend class StartupBootMenuAdmission;
  StartupBootControllerToken(
      std::shared_ptr<const StartupSceneLoadPackage> package,
      std::shared_ptr<const void> scene_lifetime, std::uint64_t owner,
      std::uint64_t component, std::uint64_t factory_generation,
      bool source_backed_factory)
      : package_(std::move(package)),
        scene_lifetime_(std::move(scene_lifetime)), owner_(owner),
        component_(component), factory_generation_(factory_generation),
        source_backed_factory_(source_backed_factory) {}

  std::shared_ptr<const StartupSceneLoadPackage> package_;
  std::shared_ptr<const void> scene_lifetime_;
  std::uint64_t owner_{};
  std::uint64_t component_{};
  std::uint64_t factory_generation_{};
  // Only StartupBootSceneFactory may attest that the construction used the
  // exact checked package/GMS directory pair. Reader admission consumes this
  // internal provenance; generic construction remains a structural test hook.
  bool source_backed_factory_{};
};

class StartupBootSceneConstruction final {
public:
  [[nodiscard]] StartupBootControllerToken
  construct(std::shared_ptr<const StartupSceneLoadPackage> package,
            const StartupBootSceneLease &scene,
            const StartupBootSceneDirectoryProof &proof,
            std::uint64_t factory_generation,
            const StartupBootSceneConstructionServices &services) const {
    return construct_impl(std::move(package), scene, proof, factory_generation,
                          services, false);
  }

private:
  friend class StartupBootSceneFactory;

  [[nodiscard]] StartupBootControllerToken
  construct_source_backed(std::shared_ptr<const StartupSceneLoadPackage> package,
                          const StartupBootSceneLease &scene,
                          const StartupBootSceneDirectoryProof &proof,
                          std::uint64_t factory_generation,
                          const StartupBootSceneConstructionServices &services) const {
    return construct_impl(std::move(package), scene, proof, factory_generation,
                          services, true);
  }

  [[nodiscard]] StartupBootControllerToken
  construct_impl(std::shared_ptr<const StartupSceneLoadPackage> package,
                 const StartupBootSceneLease &scene,
                 const StartupBootSceneDirectoryProof &proof,
                 std::uint64_t factory_generation,
                 const StartupBootSceneConstructionServices &services,
                 bool source_backed_factory) const {
    if (!package || !scene.lifetime_ || factory_generation == 0U ||
        !proof.complete_directory_mapping ||
        !proof.canonical_ordinary_window_source ||
        proof.component_identifier != "ZWINDOW_BootMenu" ||
        !std::isfinite(proof.component_parameter) ||
        proof.component_parameter != 1.0F || !services.registry_live ||
        !services.allocate_ordinary_window ||
        !services.canonical_live_window_owner ||
        !services.attach_boot_menu_component ||
        !services.live_boot_menu_component) {
      throw std::runtime_error(
          "Startup boot scene construction requires checked factories");
    }
    if (!services.registry_live()) {
      throw std::runtime_error("Startup boot scene registry is not live");
    }
    const auto owner = services.allocate_ordinary_window();
    if (owner == 0U || !services.canonical_live_window_owner(owner)) {
      throw std::runtime_error(
          "Startup boot scene factory produced no canonical window");
    }
    const auto component =
        services.attach_boot_menu_component(owner, proof.component_parameter);
    if (component == 0U || !services.live_boot_menu_component(component)) {
      throw std::runtime_error(
          "Startup boot scene factory produced no live BootMenu component");
    }
    return StartupBootControllerToken(std::move(package), scene.lifetime_,
                                      owner, component, factory_generation,
                                      source_backed_factory);
  }
};

} // namespace off::runtime
