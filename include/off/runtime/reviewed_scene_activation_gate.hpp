#pragma once

#include "off/runtime/reviewed_scene_manager_lifecycle_contract.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace off::runtime {

// A caller-owned opaque lease for a candidate whose construction has already
// completed. It carries neither a scene name nor an engine identity, so this
// boundary cannot select a retail scene or infer an allocation policy.
class StagedSceneActivationCandidate final {
 public:
  [[nodiscard]] static std::optional<StagedSceneActivationCandidate> from_staged_lease(
      std::shared_ptr<const void> staged_lease) noexcept;

 private:
  friend class ReviewedSceneActivationGate;
  friend class ReviewedSceneActivationState;

  explicit StagedSceneActivationCandidate(std::shared_ptr<const void> staged_lease)
      : staged_lease_(std::move(staged_lease)) {}

  std::shared_ptr<const void> staged_lease_;
};

// Retains only the opaque lifetime of a candidate after an explicitly supplied
// activation callback succeeded. It has no inspection, lookup, render, camera,
// input, or gameplay surface.
class ReviewedSceneActivationState final {
 public:
  [[nodiscard]] bool has_committed_candidate() const noexcept {
    return committed_candidate_.has_value();
  }

 private:
  friend class ReviewedSceneActivationGate;

  void commit(StagedSceneActivationCandidate candidate) noexcept {
    committed_candidate_ = std::move(candidate);
  }

  std::optional<StagedSceneActivationCandidate> committed_candidate_;
};

struct ReviewedSceneActivationServices final {
  // A future reviewed behavior-specific adapter owns this callback. The gate
  // does not provide a manager call, component phase, target selection, or any
  // other retail behavior.
  std::function<bool()> commit_staged_activation;
};

enum class ReviewedSceneActivationResult { rejected, committed };

// Strictly disconnected commit boundary. It can consume a loaded private
// lifecycle receipt later, but only admits the one retained categorical fact
// and never wires it into normal startup. A failed callback leaves the caller's
// candidate and the prior committed state untouched; there is no retry.
class ReviewedSceneActivationGate final {
 public:
  [[nodiscard]] ReviewedSceneActivationResult consume(
      const ReviewedSceneManagerLifecycleContract& lifecycle_receipt,
      const StagedSceneActivationCandidate& candidate,
      ReviewedSceneActivationState& state,
      const ReviewedSceneActivationServices& services);

  [[nodiscard]] bool active() const noexcept { return active_; }

 private:
  bool active_{};
};

}  // namespace off::runtime
