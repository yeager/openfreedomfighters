#pragma once

#include "off/runtime/startup_boot_scene_factory.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::runtime {

// This is an observation instrument, not a startup host.  Its values are
// deliberately synthetic and its result contains a trace rather than a live
// controller, scene, registration, or allocator handle.  Normal startup must
// not include or call this type.
enum class SyntheticStartupBootSceneProbeCall {
  registry_live,
  allocate_ordinary_window,
  canonical_live_window_owner,
  attach_boot_menu_component,
  live_boot_menu_component,
};

struct SyntheticStartupBootSceneProbeTraceEntry final {
  SyntheticStartupBootSceneProbeCall call{};
  // This ordinal comes from the checked GMS directory; it is evidence, not an
  // allocator identity.
  std::size_t source_boot_owner_directory_ordinal{};
  // These are test-instrument values supplied by the caller, never recovered
  // object-service identities.
  std::uint64_t synthetic_owner_identity{};
  std::uint64_t synthetic_component_identity{};
};

struct SyntheticStartupBootSceneProbeIds final {
  std::uint64_t synthetic_factory_generation{};
  std::uint64_t synthetic_owner_identity{};
  std::uint64_t synthetic_component_identity{};
};

class SyntheticStartupBootSceneProbeResult final {
public:
  [[nodiscard]] std::size_t
  source_boot_owner_directory_ordinal() const noexcept {
    return source_boot_owner_directory_ordinal_;
  }
  [[nodiscard]] const SyntheticStartupBootSceneProbeIds &
  synthetic_ids() const noexcept {
    return synthetic_ids_;
  }
  [[nodiscard]] const std::vector<SyntheticStartupBootSceneProbeTraceEntry> &
  trace() const noexcept {
    return trace_;
  }

private:
  friend class SyntheticStartupBootSceneProbeHost;
  SyntheticStartupBootSceneProbeResult(
      std::size_t source_boot_owner_directory_ordinal,
      SyntheticStartupBootSceneProbeIds synthetic_ids,
      std::vector<SyntheticStartupBootSceneProbeTraceEntry> trace)
      : source_boot_owner_directory_ordinal_(source_boot_owner_directory_ordinal),
        synthetic_ids_(synthetic_ids), trace_(std::move(trace)) {}

  std::size_t source_boot_owner_directory_ordinal_{};
  SyntheticStartupBootSceneProbeIds synthetic_ids_{};
  std::vector<SyntheticStartupBootSceneProbeTraceEntry> trace_;
};

// A deliberately disconnected harness for exercising the checked package +
// GMS directory boundary.  It can establish only the call order required by
// StartupBootSceneFactory.  It does not discover real allocator values; use
// its trace to compare future original-runtime observations.
class SyntheticStartupBootSceneProbeHost final {
public:
  [[nodiscard]] static SyntheticStartupBootSceneProbeResult observe(
      std::shared_ptr<const StartupSceneLoadPackage> package,
      const StartupBootSceneDirectorySource &directory,
      SyntheticStartupBootSceneProbeIds synthetic_ids) {
    if (!package || !package->factory_inputs().has_value() ||
        synthetic_ids.synthetic_factory_generation == 0U ||
        synthetic_ids.synthetic_owner_identity == 0U ||
        synthetic_ids.synthetic_component_identity == 0U) {
      throw std::runtime_error(
          "synthetic startup probe requires explicit nonzero synthetic IDs");
    }

    const auto ordinal = directory.boot_owner_directory_index();
    std::vector<SyntheticStartupBootSceneProbeTraceEntry> trace;
    const auto record = [&](SyntheticStartupBootSceneProbeCall call) {
      trace.push_back({.call = call,
                       .source_boot_owner_directory_ordinal = ordinal,
                       .synthetic_owner_identity =
                           synthetic_ids.synthetic_owner_identity,
                       .synthetic_component_identity =
                           synthetic_ids.synthetic_component_identity});
    };
    const auto synthetic_scene_lifetime =
        std::make_shared<const std::uint8_t>(0U);
    const auto scene = StartupBootSceneLease::live(synthetic_scene_lifetime);
    const StartupBootSceneConstructionServices services{
        .registry_live = [&] {
          record(SyntheticStartupBootSceneProbeCall::registry_live);
          return true;
        },
        .allocate_ordinary_window = [&] {
          record(SyntheticStartupBootSceneProbeCall::allocate_ordinary_window);
          return synthetic_ids.synthetic_owner_identity;
        },
        .canonical_live_window_owner = [&](std::uint64_t owner) {
          record(SyntheticStartupBootSceneProbeCall::canonical_live_window_owner);
          return owner == synthetic_ids.synthetic_owner_identity;
        },
        .attach_boot_menu_component = [&](std::uint64_t owner, float) {
          record(SyntheticStartupBootSceneProbeCall::attach_boot_menu_component);
          return owner == synthetic_ids.synthetic_owner_identity
                     ? synthetic_ids.synthetic_component_identity : 0U;
        },
        .live_boot_menu_component = [&](std::uint64_t component) {
          record(SyntheticStartupBootSceneProbeCall::live_boot_menu_component);
          return component == synthetic_ids.synthetic_component_identity;
        },
    };
    StartupBootSceneFactory factory;
    // Discard the token: retaining it would falsely make this probe appear to
    // own a runnable scene.
    static_cast<void>(factory.construct(std::move(package), directory, scene,
                                        synthetic_ids.synthetic_factory_generation,
                                        services));
    return SyntheticStartupBootSceneProbeResult(ordinal, synthetic_ids,
                                                std::move(trace));
  }
};

} // namespace off::runtime
