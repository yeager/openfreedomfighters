#pragma once

#include "off/runtime/startup_boot_scene_registry.hpp"
#include "off/runtime/startup_boot_menu_deferred_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::runtime {

// Source-only retention for the one checked BootMenu deferred component block.
// It copies no interpretation out of the block: component construction,
// reader dispatch, initialization, focus, input, rendering and transitions
// remain deliberately unavailable.  The checked package lease is retained so
// the copied bytes can never be paired with a different parsed GMS image.
class StartupBootMenuComponentEnvelope final {
public:
  StartupBootMenuComponentEnvelope(const StartupBootMenuComponentEnvelope &) =
      delete;
  StartupBootMenuComponentEnvelope &operator=(
      const StartupBootMenuComponentEnvelope &) = delete;
  StartupBootMenuComponentEnvelope(StartupBootMenuComponentEnvelope &&) noexcept =
      default;
  StartupBootMenuComponentEnvelope &operator=(
      StartupBootMenuComponentEnvelope &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return package_ && owner_handle_ != 0U && !deferred_source_block_.empty();
  }
  [[nodiscard]] std::size_t owner_source_directory_index() const noexcept {
    return owner_source_directory_index_;
  }
  [[nodiscard]] std::uint64_t owner_handle() const noexcept {
    return owner_handle_;
  }
  [[nodiscard]] std::span<const std::byte> deferred_source_block() const noexcept {
    return deferred_source_block_;
  }
  [[nodiscard]] const StartupBootMenuDeferredProfile &deferred_profile() const
      noexcept {
    return deferred_profile_;
  }

private:
  friend class StartupBootMenuComponentEnvelopeFactory;
  StartupBootMenuComponentEnvelope(
      std::shared_ptr<const StartupSceneLoadPackage> package,
      std::size_t owner_source_directory_index, std::uint64_t owner_handle,
      std::vector<std::byte> deferred_source_block,
      StartupBootMenuDeferredProfile deferred_profile)
      : package_(std::move(package)),
        owner_source_directory_index_(owner_source_directory_index),
        owner_handle_(owner_handle),
        deferred_source_block_(std::move(deferred_source_block)),
        deferred_profile_(std::move(deferred_profile)) {}

  std::shared_ptr<const StartupSceneLoadPackage> package_;
  std::size_t owner_source_directory_index_{};
  std::uint64_t owner_handle_{};
  std::vector<std::byte> deferred_source_block_;
  StartupBootMenuDeferredProfile deferred_profile_{};
};

class StartupBootMenuComponentEnvelopeFactory final {
public:
  [[nodiscard]] StartupBootMenuComponentEnvelope construct(
      std::shared_ptr<const StartupSceneLoadPackage> package,
      const StartupBootSceneDirectorySource &directory,
      const StartupBootSceneRegistry &registry) const {
    if (!package || !package->factory_inputs() || !registry.valid() ||
        !registry.retains_checked_package(package) ||
        !directory.matches_checked_gms(package->factory_inputs()->gms())) {
      throw std::runtime_error(
          "startup BootMenu envelope requires checked package provenance");
    }
    const auto owner_source = directory.boot_owner_directory_index();
    const auto owner_handle = registry.handle_for_source_directory(owner_source);
    if (!owner_handle || *owner_handle == 0U ||
        *owner_handle != registry.boot_menu_owner() ||
        registry.node(*owner_handle).source_directory_index != owner_source ||
        directory.proof().component_identifier != "ZWINDOW_BootMenu" ||
        directory.proof().component_parameter != registry.boot_menu_parameter()) {
      throw std::runtime_error(
          "startup BootMenu envelope registry owner is invalid");
    }
    const auto source_block =
        package->factory_inputs()->gms().deferred_source_block(owner_source);
    if (source_block.empty()) {
      throw std::runtime_error("startup BootMenu deferred source block is empty");
    }
    const auto deferred_profile =
        StartupBootMenuDeferredProfiler::profile(source_block);
    return StartupBootMenuComponentEnvelope(
        std::move(package), owner_source, *owner_handle,
        {source_block.begin(), source_block.end()}, deferred_profile);
  }
};

} // namespace off::runtime
