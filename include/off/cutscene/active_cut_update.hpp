#pragma once

#include "off/cutscene/timeline_position.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace off::cutscene {

// A retained member schedule for the bounded active-cut update model.  The
// handles are runtime-service inputs; this type does not resolve GMS names or
// construct scene objects.
struct ActiveCutMember {
  std::uint64_t target{};
  float start{};
  float end{};
  // This is the member-info Boolean consumed by the tracked end operations.
  // It suppresses only the primary zero-count operation; it never suppresses
  // accounting or record removal.
  bool end_operation_enabled{true};
};

enum class ActiveCutUpdateResult { updated, completed };

enum class ActiveCutTrackingCollection { primary, secondary };

// A resolver returns both the selected runtime identity and the retained
// source key used by the two native-compatible tracking routes.  Keeping them
// separate prevents a member-end target from being treated as a source key.
struct ActiveCutTrackingRegistration {
  std::uint64_t selected_target{};
  std::uint64_t retained_source{};
  ActiveCutTrackingCollection collection{ActiveCutTrackingCollection::primary};
};

// All side effects are supplied explicitly. A member is resolved only for its
// due start pass; member end uses the retained tracking collections instead of
// assuming that its target is a live object or source key.
struct ActiveCutUpdateServices {
  std::function<std::uint32_t()> sample_scene_clock;
  std::function<std::optional<ActiveCutTrackingRegistration>(std::uint64_t)> resolve_member;
  std::function<void(std::uint64_t, std::size_t)> start_member;
  // The primary operation owns any source resolution it needs.  The secondary
  // route is deliberately different: its source must resolve first.
  std::function<void(std::uint64_t, std::uint64_t, std::size_t, bool)> end_primary_member;
  std::function<std::optional<std::uint64_t>(std::uint64_t)> resolve_retained_source;
  std::function<void(std::uint64_t, std::uint64_t, std::size_t, bool)> end_secondary_member;
  std::function<void(std::uint64_t)> complete;
};

// A bounded active-cut state machine, not an automatic intro player.  It
// samples the supplied scene clock once per update, performs independently
// ordered strict start/end passes, and invokes completion synchronously after
// cleanup.  Callbacks may request an end through request_end(); reentrant
// update/start operations are intentionally rejected as a safety boundary.
class ActiveCutUpdate final {
public:
  explicit ActiveCutUpdate(std::vector<ActiveCutMember> members, float natural_end)
      : members_(std::move(members)), natural_end_(natural_end) {
    if (!std::isfinite(natural_end_)) {
      throw std::runtime_error("active cut natural end must be finite");
    }
    for (const auto& member : members_) {
      if (member.target == 0U || !std::isfinite(member.start) || !std::isfinite(member.end)) {
        throw std::runtime_error("active cut member requires a live target and finite bounds");
      }
    }
    start_order_ = ordered_by(&ActiveCutMember::start);
    end_order_ = ordered_by(&ActiveCutMember::end);
    started_.assign(members_.size(), false);
    ended_.assign(members_.size(), false);
  }

  ActiveCutUpdate(const ActiveCutUpdate&) = delete;
  ActiveCutUpdate& operator=(const ActiveCutUpdate&) = delete;
  ActiveCutUpdate(ActiveCutUpdate&&) = delete;
  ActiveCutUpdate& operator=(ActiveCutUpdate&&) = delete;

  void start(std::uint32_t scene_clock_start, std::optional<std::uint64_t> caller = std::nullopt) {
    if (active_ || updating_) throw std::runtime_error("active cut start requires an inactive player");
    scene_clock_start_ = scene_clock_start;
    caller_ = caller;
    pending_end_ = false;
    std::fill(started_.begin(), started_.end(), false);
    std::fill(ended_.begin(), ended_.end(), false);
    primary_tracking_.clear();
    secondary_tracking_.clear();
    active_ = true;
  }

  // This records a request only.  The update checks it after both member
  // passes, then before the natural-end tail.
  void request_end() {
    if (!active_) throw std::runtime_error("active cut end request requires an active player");
    pending_end_ = true;
  }

  [[nodiscard]] ActiveCutUpdateResult update(const ActiveCutUpdateServices& services) {
    if (!active_ || updating_ || !services.sample_scene_clock || !services.resolve_member ||
        !services.start_member || !services.end_primary_member || !services.resolve_retained_source ||
        !services.end_secondary_member || !services.complete) {
      throw std::runtime_error("active cut update requires an active player and all services");
    }
    struct UpdateGuard {
      bool& value;
      explicit UpdateGuard(bool& supplied) : value(supplied) { value = true; }
      ~UpdateGuard() { value = false; }
    } guard(updating_);

    const auto position = timeline_position(services.sample_scene_clock(), scene_clock_start_);
    run_start_pass(position, services);
    run_end_pass(position, services);

    if (pending_end_) {
      cleanup(services);
      return ActiveCutUpdateResult::completed;
    }
    // A natural end is deliberately only a request in this update.  It is
    // observed by the next update, avoiding a second cleanup after the passes.
    if (position > natural_end_) pending_end_ = true;
    return ActiveCutUpdateResult::updated;
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] bool pending_end() const noexcept { return pending_end_; }
  [[nodiscard]] std::optional<std::uint64_t> caller() const noexcept { return caller_; }

private:
  using Bound = float ActiveCutMember::*;

  [[nodiscard]] std::vector<std::size_t> ordered_by(Bound bound) const {
    std::vector<std::size_t> result;
    result.reserve(members_.size());
    for (std::size_t index = 0; index < members_.size(); ++index) result.push_back(index);
    std::stable_sort(result.begin(), result.end(), [&](std::size_t left, std::size_t right) {
      return members_[left].*bound < members_[right].*bound;
    });
    return result;
  }

  struct TrackingEntry {
    std::uint64_t selected_target{};
    std::uint64_t retained_source{};
    std::size_t index{};
    std::int32_t references{};
  };

  void run_start_pass(float position, const ActiveCutUpdateServices& services) {
    for (const auto index : start_order_) {
      if (started_[index] || !(position > members_[index].start)) continue;
      const auto resolved = services.resolve_member(members_[index].target);
      if (!resolved) continue;
      if (resolved->selected_target == 0U || resolved->retained_source == 0U) {
        throw std::runtime_error("active cut member resolver returned an invalid tracking registration");
      }
      // Both collections are consulted before creating a new selected
      // identity. Tracking is retained before the post-callback fired flag.
      const auto selected_target = retain(*resolved, index);
      services.start_member(selected_target, index);
      started_[index] = true;
    }
  }

  void run_end_pass(float position, const ActiveCutUpdateServices& services) {
    for (const auto index : end_order_) {
      if (!started_[index] || ended_[index] || !(position > members_[index].end)) continue;
      end_tracked_member(members_[index], index, services);
      ended_[index] = true;
    }
  }

  void cleanup(const ActiveCutUpdateServices& services) {
    // Reacquire the front after every callback. The public model rejects
    // reentrant update/start calls, but this avoids iterator lifetime claims
    // across service calls and preserves callback-before-removal ordering.
    while (!primary_tracking_.empty()) {
      const auto entry = primary_tracking_.front();
      services.end_primary_member(entry.selected_target, entry.retained_source, entry.index, true);
      erase_primary(entry.retained_source);
    }
    while (!secondary_tracking_.empty()) {
      const auto entry = secondary_tracking_.front();
      if (const auto resolved = services.resolve_retained_source(entry.retained_source)) {
        services.end_secondary_member(*resolved, entry.selected_target, entry.index, true);
      }
      erase_secondary(entry.retained_source);
    }
    std::fill(started_.begin(), started_.end(), false);
    std::fill(ended_.begin(), ended_.end(), false);
    pending_end_ = false;
    active_ = false;
    const auto completion_caller = caller_.value_or(0U);
    services.complete(completion_caller);
    caller_.reset();
  }

  [[nodiscard]] std::uint64_t retain(const ActiveCutTrackingRegistration& registration, std::size_t index) {
    const auto retain_existing = [&](std::vector<TrackingEntry>& collection) -> std::optional<std::uint64_t> {
      const auto found = std::find_if(collection.begin(), collection.end(), [&](const TrackingEntry& entry) {
        return entry.retained_source == registration.retained_source;
      });
      if (found == collection.end()) return std::nullopt;
      if (found->references == std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("active cut tracking reference count overflow");
      }
      ++found->references;
      return found->selected_target;
    };
    if (const auto selected = retain_existing(primary_tracking_)) return *selected;
    if (const auto selected = retain_existing(secondary_tracking_)) return *selected;
    auto& collection = registration.collection == ActiveCutTrackingCollection::primary
                           ? primary_tracking_
                           : secondary_tracking_;
    collection.push_back({registration.selected_target, registration.retained_source, index, 1});
    return registration.selected_target;
  }

  void end_tracked_member(const ActiveCutMember& member, std::size_t index,
                          const ActiveCutUpdateServices& services) {
    const auto primary = std::find_if(primary_tracking_.begin(), primary_tracking_.end(), [&](const TrackingEntry& entry) {
      return entry.retained_source == member.target;
    });
    if (primary != primary_tracking_.end()) {
      decrement_primary(*primary, member.end_operation_enabled, services);
      return;
    }
    const auto secondary = std::find_if(secondary_tracking_.begin(), secondary_tracking_.end(), [&](const TrackingEntry& entry) {
      return entry.selected_target == member.target;
    });
    if (secondary == secondary_tracking_.end()) return;
    decrement_secondary(*secondary, member.end_operation_enabled, services);
  }

  void decrement_primary(const TrackingEntry& entry, bool enabled,
                         const ActiveCutUpdateServices& services) {
    auto found = std::find_if(primary_tracking_.begin(), primary_tracking_.end(), [&](const TrackingEntry& current) {
      return current.retained_source == entry.retained_source;
    });
    if (found == primary_tracking_.end()) return;
    if (found->references <= 0) {
      throw std::runtime_error("active cut primary tracking reference count underflow");
    }
    if (--found->references != 0) return;
    // The entry remains present while the operation runs.
    if (enabled) services.end_primary_member(found->selected_target, found->retained_source, found->index, enabled);
    erase_primary(found->retained_source);
  }

  void decrement_secondary(const TrackingEntry& entry, bool enabled,
                           const ActiveCutUpdateServices& services) {
    auto found = std::find_if(secondary_tracking_.begin(), secondary_tracking_.end(), [&](const TrackingEntry& current) {
      return current.selected_target == entry.selected_target;
    });
    if (found == secondary_tracking_.end()) return;
    if (found->references <= 0) {
      throw std::runtime_error("active cut secondary tracking reference count underflow");
    }
    if (--found->references != 0) return;
    const auto retained_source = found->retained_source;
    const auto selected_target = found->selected_target;
    const auto tracking_index = found->index;
    if (const auto resolved = services.resolve_retained_source(retained_source)) {
      services.end_secondary_member(*resolved, selected_target, tracking_index, enabled);
    }
    erase_secondary(retained_source);
  }

  void erase_primary(std::uint64_t retained_source) {
    const auto found = std::find_if(primary_tracking_.begin(), primary_tracking_.end(), [&](const TrackingEntry& entry) {
      return entry.retained_source == retained_source;
    });
    if (found != primary_tracking_.end()) primary_tracking_.erase(found);
  }

  void erase_secondary(std::uint64_t retained_source) {
    const auto found = std::find_if(secondary_tracking_.begin(), secondary_tracking_.end(), [&](const TrackingEntry& entry) {
      return entry.retained_source == retained_source;
    });
    if (found != secondary_tracking_.end()) secondary_tracking_.erase(found);
  }

  std::vector<ActiveCutMember> members_;
  std::vector<std::size_t> start_order_;
  std::vector<std::size_t> end_order_;
  std::vector<bool> started_;
  std::vector<bool> ended_;
  std::vector<TrackingEntry> primary_tracking_;
  std::vector<TrackingEntry> secondary_tracking_;
  float natural_end_{};
  std::uint32_t scene_clock_start_{};
  std::optional<std::uint64_t> caller_;
  bool active_{};
  bool pending_end_{};
  bool updating_{};
};

} // namespace off::cutscene
