#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace off::runtime {

class StartupSceneLoader;

// This is a checked handoff from a caller-owned, scene-specific parser. It is
// deliberately not a GMS reader and does not make a scene target loadable.
class StartLoaderLoadScreenSource final {
public:
  [[nodiscard]] static StartLoaderLoadScreenSource from_parsed_attachment(
      std::string_view attachment_identifier, float attachment_parameter,
      std::string_view target, bool exact_source_wrapper_consumed) {
    if (attachment_identifier != "ZWINGROUP_LoadScreen" ||
        !std::isfinite(attachment_parameter) || attachment_parameter != 0.0F ||
        !exact_source_wrapper_consumed || target != "FF-Startup") {
      throw std::runtime_error("unsupported StartLoader LoadScreen source");
    }
    return StartLoaderLoadScreenSource(std::string(target));
  }

  [[nodiscard]] std::string_view target() const noexcept { return target_; }

private:
  explicit StartLoaderLoadScreenSource(std::string target)
      : target_(std::move(target)) {}

  std::string target_;
};

struct DeferredSceneEntry {
  std::uint64_t identity{};
  bool removal_requested{};
};

// Request-only scene-manager boundary. It never reads an archive, creates a
// scene, changes a renderer, or consumes input.
class SceneTransitionQueue final {
public:
  void retain_scene_entry(std::uint64_t identity) {
    if (identity == 0U) {
      throw std::runtime_error("scene entry identity must be nonzero");
    }
    entries_.push_back({identity, false});
  }

  void set_current_scene(std::uint64_t identity) {
    if (identity == 0U) {
      throw std::runtime_error("current scene identity must be nonzero");
    }
    current_scene_ = identity;
  }

  void request_clear() noexcept {
    for (auto& entry : entries_) {
      entry.removal_requested = true;
    }
    current_scene_.reset();
    pending_ = true;
    ++clear_requests_;
  }

  // Returns false for the native null/empty request case. A successful return
  // means only that a normalized request record was retained.
  [[nodiscard]] bool request_target(std::string_view target) {
    if (target.empty()) {
      return false;
    }
    std::string normalized(target);
    for (auto& character : normalized) {
      if (character == '/') {
        character = '\\';
      }
    }
    targets_.push_back(std::move(normalized));
    pending_ = true;
    return true;
  }

  [[nodiscard]] const std::vector<DeferredSceneEntry>& entries() const noexcept {
    return entries_;
  }
  [[nodiscard]] const std::vector<std::string>& targets() const noexcept {
    return targets_;
  }
  [[nodiscard]] const std::optional<std::uint64_t>& current_scene() const noexcept {
    return current_scene_;
  }
  [[nodiscard]] bool pending() const noexcept { return pending_; }
  [[nodiscard]] std::uint32_t clear_requests() const noexcept {
    return clear_requests_;
  }

private:
  friend class StartupSceneLoader;

  void commit_supported_transition() noexcept {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [](const DeferredSceneEntry& entry) {
                                    return entry.removal_requested;
                                  }),
                   entries_.end());
    targets_.clear();
    pending_ = false;
  }

  std::vector<DeferredSceneEntry> entries_;
  std::vector<std::string> targets_;
  std::optional<std::uint64_t> current_scene_;
  bool pending_{};
  std::uint32_t clear_requests_{};
};

// Models the proven request boundary only. Its caller must establish ordinary
// component admission and the source of the initial counter/setup state.
class LoadScreenTransition final {
public:
  explicit LoadScreenTransition(StartLoaderLoadScreenSource source,
                                std::uint32_t initial_update_count = 0U,
                                bool one_time_setup_pending = true)
      : target_(source.target()), update_count_(initial_update_count),
        one_time_setup_pending_(one_time_setup_pending) {}

  // Returns true only when this call retained a nonempty target request.
  [[nodiscard]] bool ordinary_update(
      SceneTransitionQueue& queue,
      const std::function<void()>& one_time_setup = {}) {
    if (one_time_setup_pending_) {
      if (!one_time_setup) {
        throw std::runtime_error("LoadScreen one-time setup service is unavailable");
      }
      one_time_setup();
      one_time_setup_pending_ = false;
    }
    ++update_count_;
    if (update_count_ < 3U) {
      return false;
    }
    queue.request_clear();
    const bool retained = queue.request_target(target_);
    target_.clear();
    return retained;
  }

  [[nodiscard]] std::uint32_t update_count() const noexcept { return update_count_; }
  [[nodiscard]] bool one_time_setup_pending() const noexcept {
    return one_time_setup_pending_;
  }
  [[nodiscard]] std::string_view retained_target() const noexcept { return target_; }

private:
  std::string target_;
  std::uint32_t update_count_{};
  bool one_time_setup_pending_{};
};

}  // namespace off::runtime
