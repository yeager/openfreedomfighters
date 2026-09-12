#pragma once

#include "off/data/gms_image.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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
  // `GmsImage::startloader_load_screen_source()` has already established the
  // one supported owner/attachment grammar. This handoff still only retains a
  // target string; it does not enqueue or construct a scene.
  [[nodiscard]] static StartLoaderLoadScreenSource from_parsed_source(
      const data::GmsStartLoaderLoadScreenSource& source) {
    if (source.target != "FF-Startup") {
      throw std::runtime_error("unsupported StartLoader LoadScreen source");
    }
    return StartLoaderLoadScreenSource(source.target, source.directory_index);
  }

  [[nodiscard]] static StartLoaderLoadScreenSource from_parsed_attachment(
      std::string_view attachment_identifier, float attachment_parameter,
      std::string_view target, bool exact_source_wrapper_consumed) {
    if (attachment_identifier != "ZWINGROUP_LoadScreen" ||
        !std::isfinite(attachment_parameter) || attachment_parameter != 0.0F ||
        !exact_source_wrapper_consumed || target != "FF-Startup") {
      throw std::runtime_error("unsupported StartLoader LoadScreen source");
    }
    return StartLoaderLoadScreenSource(std::string(target), std::nullopt);
  }

  [[nodiscard]] std::string_view target() const noexcept { return target_; }
  // Present only when the source came from the typed GMS parser. It is source
  // provenance, not a runtime object identity or factory handle.
  [[nodiscard]] const std::optional<std::size_t> &
  parsed_directory_index() const noexcept {
    return parsed_directory_index_;
  }

private:
  explicit StartLoaderLoadScreenSource(
      std::string target, std::optional<std::size_t> parsed_directory_index)
      : target_(std::move(target)),
        parsed_directory_index_(parsed_directory_index) {}

  std::string target_;
  std::optional<std::size_t> parsed_directory_index_;
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
    // A supported replacement must retain exactly one target *after* its
    // clear request.  Keeping the offset makes a target-only request, or one
    // queued before the clear, distinguishable without inventing a generic
    // manager ordering policy.
    // Once the proven clear-then-one-target handoff exists, the component's
    // later empty-target updates must not invalidate it before the manager
    // gets a chance to consume the request.  A target that existed before
    // its first clear still retains a nonzero offset and remains rejected.
    if (!clear_target_offset_.has_value() || *clear_target_offset_ != 0U ||
        targets_.size() != 1U) {
      clear_target_offset_ = targets_.size();
    }
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

  // The LoadScreen component's clear-plus-target request is one logical
  // handoff. Stage the only allocating operation before retiring the current
  // scene so an allocation failure cannot leave an orphaned clear request.
  // Empty targets retain the observed clear-only behavior.
  [[nodiscard]] bool request_clear_then_target(std::string_view target) {
    if (target.empty()) {
      request_clear();
      return false;
    }
    std::string normalized(target);
    for (auto& character : normalized) {
      if (character == '/') {
        character = '\\';
      }
    }
    auto staged_targets = targets_;
    staged_targets.push_back(std::move(normalized));

    for (auto& entry : entries_) {
      entry.removal_requested = true;
    }
    current_scene_.reset();
    targets_ = std::move(staged_targets);
    pending_ = true;
    clear_target_offset_ = targets_.size() - 1U;
    ++clear_requests_;
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

  // Loader callbacks are intentionally given no queue reference, but a host
  // integration can still capture one. Keep a private checkpoint so a
  // candidate factory cannot smuggle a second request, retire entries, or
  // otherwise change the request it is meant to satisfy. The loader restores
  // this exact state before rejecting that candidate.
  struct Checkpoint {
    std::vector<DeferredSceneEntry> entries;
    std::vector<std::string> targets;
    std::optional<std::uint64_t> current_scene;
    bool pending{};
    std::uint32_t clear_requests{};
    std::optional<std::size_t> clear_target_offset;
  };

  [[nodiscard]] Checkpoint checkpoint() const {
    return {entries_, targets_, current_scene_, pending_, clear_requests_,
            clear_target_offset_};
  }

  [[nodiscard]] bool matches(const Checkpoint& checkpoint) const noexcept {
    if (current_scene_ != checkpoint.current_scene || pending_ != checkpoint.pending ||
        clear_requests_ != checkpoint.clear_requests || targets_ != checkpoint.targets ||
        clear_target_offset_ != checkpoint.clear_target_offset ||
        entries_.size() != checkpoint.entries.size()) {
      return false;
    }
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      if (entries_[index].identity != checkpoint.entries[index].identity ||
          entries_[index].removal_requested !=
              checkpoint.entries[index].removal_requested) {
        return false;
      }
    }
    return true;
  }

  void restore(Checkpoint checkpoint) noexcept {
    entries_ = std::move(checkpoint.entries);
    targets_ = std::move(checkpoint.targets);
    current_scene_ = checkpoint.current_scene;
    pending_ = checkpoint.pending;
    clear_requests_ = checkpoint.clear_requests;
    clear_target_offset_ = checkpoint.clear_target_offset;
  }

  void commit_supported_transition() noexcept {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [](const DeferredSceneEntry& entry) {
                                    return entry.removal_requested;
                                  }),
                   entries_.end());
    targets_.clear();
    pending_ = false;
    clear_target_offset_.reset();
  }

  std::vector<DeferredSceneEntry> entries_;
  std::vector<std::string> targets_;
  std::optional<std::uint64_t> current_scene_;
  bool pending_{};
  std::uint32_t clear_requests_{};
  std::optional<std::size_t> clear_target_offset_;
};

// Models the proven request boundary only. Its caller must establish ordinary
// component admission and the source of the initial counter/setup state.
class LoadScreenTransition final {
public:
  explicit LoadScreenTransition(StartLoaderLoadScreenSource source,
                                std::uint32_t initial_update_count = 0U,
                                bool one_time_setup_pending = true)
      : target_(source.target()), update_count_(initial_update_count),
        one_time_setup_pending_(one_time_setup_pending),
        source_directory_index_(source.parsed_directory_index()) {}

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
    // A recovered counter may already be at its maximum. Saturate rather than
    // wrapping it back below the third-update gate and delaying a known
    // request by three arbitrary frames.
    if (update_count_ != std::numeric_limits<std::uint32_t>::max()) {
      ++update_count_;
    }
    if (update_count_ < 3U) {
      return false;
    }
    const bool retained = queue.request_clear_then_target(target_);
    target_.clear();
    return retained;
  }

  [[nodiscard]] std::uint32_t update_count() const noexcept { return update_count_; }
  [[nodiscard]] bool one_time_setup_pending() const noexcept {
    return one_time_setup_pending_;
  }
  [[nodiscard]] std::string_view retained_target() const noexcept { return target_; }
  [[nodiscard]] const std::optional<std::size_t> &
  source_directory_index() const noexcept {
    return source_directory_index_;
  }

private:
  std::string target_;
  std::uint32_t update_count_{};
  bool one_time_setup_pending_{};
  std::optional<std::size_t> source_directory_index_;
};

}  // namespace off::runtime
