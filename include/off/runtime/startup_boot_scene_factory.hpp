#pragma once

#include "off/runtime/startup_boot_scene_construction.hpp"
#include "off/runtime/startup_boot_scene_directory_source.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>

namespace off::runtime {

// Narrow bridge from a checked FF-StartUp source package to the already
// bounded BootMenu construction.  It is intentionally disconnected from the
// startup loader: callers provide the live scene lease, factory generation and
// runtime services, and this type has no transition, lifecycle, input or
// rendering policy.
class StartupBootSceneFactory final {
public:
  [[nodiscard]] StartupBootControllerToken
  construct(std::shared_ptr<const StartupSceneLoadPackage> package,
            const StartupBootSceneDirectorySource &directory_source,
            const StartupBootSceneLease &scene,
            std::uint64_t factory_generation,
            const StartupBootSceneConstructionServices &services) const {
    if (!package || !package->factory_inputs().has_value()) {
      throw std::runtime_error(
          "Startup boot scene factory requires source-backed package inputs");
    }
    const auto &inputs = *package->factory_inputs();
    if (!directory_source.matches_checked_gms(inputs.gms())) {
      throw std::runtime_error(
          "Startup boot scene factory source and package GMS differ");
    }
    return construction_.construct_source_backed(
        std::move(package), scene, directory_source.proof(),
        factory_generation, services);
  }

private:
  StartupBootSceneConstruction construction_;
};

} // namespace off::runtime
