#include "off/runtime/reviewed_scene_activation_gate.hpp"

namespace off::runtime {

std::optional<StagedSceneActivationCandidate>
StagedSceneActivationCandidate::from_staged_lease(
    std::shared_ptr<const void> staged_lease) noexcept {
  if (!staged_lease) return std::nullopt;
  return StagedSceneActivationCandidate{std::move(staged_lease)};
}

ReviewedSceneActivationResult ReviewedSceneActivationGate::consume(
    const ReviewedSceneManagerLifecycleContract& lifecycle_receipt,
    const StagedSceneActivationCandidate& candidate,
    ReviewedSceneActivationState& state,
    const ReviewedSceneActivationServices& services) {
  if (active_) {
    throw std::runtime_error("ReviewedSceneActivationGate is nonreentrant");
  }
  // The enum comparison deliberately makes admission explicit at this narrow
  // seam, even though currently-loaded contracts have one possible receipt.
  if (!lifecycle_receipt.admitted() ||
      lifecycle_receipt.receipt() !=
          SceneManagerLifecycleReceipt::repeated_success_with_distinct_failure ||
      state.has_committed_candidate() || !candidate.staged_lease_ ||
      !services.commit_staged_activation) {
    return ReviewedSceneActivationResult::rejected;
  }

  active_ = true;
  try {
    if (!services.commit_staged_activation()) {
      active_ = false;
      return ReviewedSceneActivationResult::rejected;
    }
    state.commit(candidate);
    active_ = false;
    return ReviewedSceneActivationResult::committed;
  } catch (...) {
    active_ = false;
    throw;
  }
}

}  // namespace off::runtime
