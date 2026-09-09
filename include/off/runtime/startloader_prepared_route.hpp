#pragma once

#include "off/data/gms_image.hpp"
#include "off/runtime/startloader_load_screen.hpp"
#include "off/runtime/startup_scene_package_source.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace off::runtime {

// These are explicit because their production construction has not been
// reconstructed. The only admitted construction starts at the reviewed state:
// setup pending and update count zero.
struct StartLoaderPreparedRouteState final {
  std::uint32_t initial_update_count{};
  bool one_time_setup_pending{};
};

enum class StartLoaderPreparedRouteResult { awaiting_target, package_prepared };

// A small, production-independent bridge between a caller-owned checked
// StartLoader GMS image and a checked FF-StartUp package. It deliberately
// stops after package preparation: it neither invokes a scene factory nor
// mutates a manager-owned scene state.
class StartLoaderPreparedRoute final {
public:
  [[nodiscard]] static StartLoaderPreparedRoute
  from_checked_gms(const data::GmsImage &startloader_gms,
                   const std::filesystem::path &startup_package_path,
                   StartLoaderPreparedRouteState state) {
    if (state.initial_update_count != 0U || !state.one_time_setup_pending) {
      throw std::runtime_error(
          "StartLoader prepared route requires the reviewed initial state");
    }
    if (startup_package_path.filename() != "FF-StartUp.ZIP") {
      throw std::runtime_error("StartLoader prepared route requires the "
                               "canonical FF-StartUp package");
    }

    // The GMS parser, rather than a caller-provided target string or wrapper,
    // is the sole admission source for this route.
    const auto source = StartLoaderLoadScreenSource::from_parsed_source(
        startloader_gms.startloader_load_screen_source());
    return StartLoaderPreparedRoute(source, startup_package_path, state);
  }

  [[nodiscard]] StartLoaderPreparedRouteResult
  ordinary_update(const std::function<void()> &one_time_setup) {
    if (prepared_package_.has_value()) {
      return StartLoaderPreparedRouteResult::package_prepared;
    }

    const bool retained_target =
        transition_.ordinary_update(transitions_, one_time_setup);
    if (!retained_target) {
      return StartLoaderPreparedRouteResult::awaiting_target;
    }
    if (transitions_.targets().size() != 1U ||
        transitions_.targets().front() != "FF-Startup") {
      throw std::runtime_error(
          "StartLoader prepared route retained an invalid target");
    }

    prepared_package_.emplace(StartupScenePackageSource::prepare_checked(
        transitions_.targets().front(), startup_package_path_));
    return StartLoaderPreparedRouteResult::package_prepared;
  }

  [[nodiscard]] std::uint32_t update_count() const noexcept {
    return transition_.update_count();
  }
  [[nodiscard]] bool one_time_setup_pending() const noexcept {
    return transition_.one_time_setup_pending();
  }
  [[nodiscard]] bool has_prepared_package() const noexcept {
    return prepared_package_.has_value();
  }
  [[nodiscard]] const std::optional<StartupSceneLoadPackage> &
  prepared_package() const noexcept {
    return prepared_package_;
  }

private:
  StartLoaderPreparedRoute(StartLoaderLoadScreenSource source,
                           std::filesystem::path startup_package_path,
                           StartLoaderPreparedRouteState state)
      : transition_(std::move(source), state.initial_update_count,
                    state.one_time_setup_pending),
        startup_package_path_(std::move(startup_package_path)) {}

  LoadScreenTransition transition_;
  SceneTransitionQueue transitions_;
  std::filesystem::path startup_package_path_;
  std::optional<StartupSceneLoadPackage> prepared_package_;
};

} // namespace off::runtime
