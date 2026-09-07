#pragma once

#include "off/cutscene/timeline_position.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
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
};

enum class ActiveCutUpdateResult { updated, completed };

// All side effects are supplied explicitly.  A target is re-resolved for each
// due start/end pass, so a member never implies a retained live object.
struct ActiveCutUpdateServices {
  std::function<std::uint32_t()> sample_scene_clock;
  std::function<std::optional<std::uint64_t>(std::uint64_t)> resolve_member;
  std::function<void(std::uint64_t, std::size_t)> start_member;
  std::function<void(std::uint64_t, std::size_t)> end_member;
  std::function<void(std::uint64_t, std::size_t)> cleanup_started_member;
  std::function<void(std::uint64_t, std::size_t)> cleanup_ended_member;
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
    started_tracking_.clear();
    ended_tracking_.clear();
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
        !services.start_member || !services.end_member || !services.cleanup_started_member ||
        !services.cleanup_ended_member || !services.complete) {
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

  void run_start_pass(float position, const ActiveCutUpdateServices& services) {
    for (const auto index : start_order_) {
      if (started_[index] || !(position > members_[index].start)) continue;
      const auto resolved = services.resolve_member(members_[index].target);
      if (!resolved) continue;
      services.start_member(*resolved, index);
      // Tracking is retained before the post-callback fired flag.
      started_tracking_.push_back({*resolved, index});
      started_[index] = true;
    }
  }

  void run_end_pass(float position, const ActiveCutUpdateServices& services) {
    for (const auto index : end_order_) {
      if (!started_[index] || ended_[index] || !(position > members_[index].end)) continue;
      const auto resolved = services.resolve_member(members_[index].target);
      if (!resolved) continue;
      services.end_member(*resolved, index);
      // A separate retained collection models the second cleanup reference.
      ended_tracking_.push_back({*resolved, index});
      ended_[index] = true;
    }
  }

  void cleanup(const ActiveCutUpdateServices& services) {
    for (const auto& [target, index] : ended_tracking_) services.cleanup_ended_member(target, index);
    for (const auto& [target, index] : started_tracking_) services.cleanup_started_member(target, index);
    ended_tracking_.clear();
    started_tracking_.clear();
    std::fill(started_.begin(), started_.end(), false);
    std::fill(ended_.begin(), ended_.end(), false);
    pending_end_ = false;
    active_ = false;
    const auto completion_caller = caller_.value_or(0U);
    services.complete(completion_caller);
    caller_.reset();
  }

  std::vector<ActiveCutMember> members_;
  std::vector<std::size_t> start_order_;
  std::vector<std::size_t> end_order_;
  std::vector<bool> started_;
  std::vector<bool> ended_;
  std::vector<std::pair<std::uint64_t, std::size_t>> started_tracking_;
  std::vector<std::pair<std::uint64_t, std::size_t>> ended_tracking_;
  float natural_end_{};
  std::uint32_t scene_clock_start_{};
  std::optional<std::uint64_t> caller_;
  bool active_{};
  bool pending_end_{};
  bool updating_{};
};

} // namespace off::cutscene
