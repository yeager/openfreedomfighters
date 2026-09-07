#pragma once

#include "off/runtime/startloader_load_screen.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace off::runtime {

// Opaque, caller-owned leases retain the checked archive, GMS source and SUP
// input through a construction attempt. This is deliberately not a parser.
class StartupSceneLoadPackage final {
public:
  [[nodiscard]] static StartupSceneLoadPackage complete(
      std::string_view target, std::shared_ptr<const void> archive_lease,
      std::shared_ptr<const void> source_lease,
      std::shared_ptr<const void> support_lease) {
    if (target != "FF-Startup" || !archive_lease || !source_lease ||
        !support_lease) {
      throw std::runtime_error("Startup scene package is incomplete");
    }
    return StartupSceneLoadPackage(std::move(archive_lease),
                                   std::move(source_lease),
                                   std::move(support_lease));
  }

private:
  explicit StartupSceneLoadPackage(std::shared_ptr<const void> archive_lease,
                                   std::shared_ptr<const void> source_lease,
                                   std::shared_ptr<const void> support_lease)
      : archive_lease_(std::move(archive_lease)),
        source_lease_(std::move(source_lease)),
        support_lease_(std::move(support_lease)) {}

  std::shared_ptr<const void> archive_lease_;
  std::shared_ptr<const void> source_lease_;
  std::shared_ptr<const void> support_lease_;
};

// A factory must explicitly produce this nonzero opaque token. It cannot turn a
// parsed package, a renderer plan, or a menu widget into a live scene.
class StartupLiveScene final {
public:
  [[nodiscard]] static StartupLiveScene from_factory(std::uint64_t identity,
                                                       std::shared_ptr<const void> lease) {
    if (identity == 0U || !lease) {
      throw std::runtime_error("Startup scene factory produced no live scene");
    }
    return StartupLiveScene(identity, std::move(lease));
  }

  [[nodiscard]] std::uint64_t identity() const noexcept { return identity_; }

private:
  explicit StartupLiveScene(std::uint64_t identity, std::shared_ptr<const void> lease)
      : identity_(identity), lease_(std::move(lease)) {}

  std::uint64_t identity_{};
  std::shared_ptr<const void> lease_;
};

// Owns the committed source package for as long as its factory-produced scene
// remains current. It has no render, input, or transition policy.
class StartupSceneLoadState final {
public:
  [[nodiscard]] const std::optional<StartupLiveScene>& current_scene() const noexcept {
    return current_scene_;
  }

private:
  friend class StartupSceneLoader;

  void commit(StartupSceneLoadPackage package, StartupLiveScene scene) {
    package_ = std::move(package);
    current_scene_ = std::move(scene);
  }

  std::optional<StartupSceneLoadPackage> package_;
  std::optional<StartupLiveScene> current_scene_;
};

struct StartupSceneLoaderServices {
  // This callback must resolve the checked owned archive and fully prepare its
  // required source and support inputs before returning a package.
  std::function<std::optional<StartupSceneLoadPackage>(std::string_view)>
      prepare_complete_checked_package;
  // The concrete FF-StartUp reader/factory is a separate required service.
  std::function<std::optional<StartupLiveScene>(const StartupSceneLoadPackage&)>
      construct_live_scene;
};

enum class StartupSceneLoaderResult { no_pending, rejected, committed };

// Disconnected replacement transaction for the one proven target. It preserves
// the request and the previously committed scene on every failed pre-commit
// boundary. No automatic retry is performed.
class StartupSceneLoader final {
public:
  [[nodiscard]] StartupSceneLoaderResult consume(
      SceneTransitionQueue& queue, StartupSceneLoadState& state,
      const StartupSceneLoaderServices& services) {
    if (active_) throw std::runtime_error("StartupSceneLoader is nonreentrant");
    if (!queue.pending_) return StartupSceneLoaderResult::no_pending;

    active_ = true;
    try {
      if (queue.targets_.size() != 1U || queue.targets_.front() != "FF-Startup" ||
          !services.prepare_complete_checked_package ||
          !services.construct_live_scene) {
        active_ = false;
        return StartupSceneLoaderResult::rejected;
      }

      auto package = services.prepare_complete_checked_package(queue.targets_.front());
      if (!package.has_value()) {
        active_ = false;
        return StartupSceneLoaderResult::rejected;
      }
      auto scene = services.construct_live_scene(*package);
      if (!scene.has_value()) {
        active_ = false;
        return StartupSceneLoaderResult::rejected;
      }

      // Everything that may fail has completed. Commit replacement ownership
      // and retire only the deferred entries now.
      state.commit(std::move(*package), std::move(*scene));
      queue.commit_supported_transition();
      active_ = false;
      return StartupSceneLoaderResult::committed;
    } catch (...) {
      active_ = false;
      throw;
    }
  }

  [[nodiscard]] bool active() const noexcept { return active_; }

private:
  bool active_{};
};

}  // namespace off::runtime
