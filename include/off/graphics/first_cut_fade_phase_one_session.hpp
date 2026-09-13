#pragma once

#include "off/graphics/intro_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace off::graphics {

class IntroRuntime;

// A deliberately isolated diagnostic pass over the three reviewed first-cut
// FadeToBlack receipts.  It is neither a global lifecycle phase nor a cut
// activation: it cannot reach MovieControl, clocks, audio, frame submission,
// or renderer admission.  Normal startup never creates this type.
enum class FirstCutFadePhaseOneSessionStage : std::uint8_t {
  cold,
  executing,
  complete,
  failed,
};

struct FirstCutFadePhaseOneReceipt final {
  std::size_t source_directory_index{};
  std::size_t component_index{};
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
};

class FirstCutFadePhaseOneSession final {
public:
  FirstCutFadePhaseOneSession(const FirstCutFadePhaseOneSession&) = delete;
  FirstCutFadePhaseOneSession& operator=(const FirstCutFadePhaseOneSession&) = delete;
  FirstCutFadePhaseOneSession(FirstCutFadePhaseOneSession&&) = delete;
  FirstCutFadePhaseOneSession& operator=(FirstCutFadePhaseOneSession&&) = delete;

  // Executes the already recovered low-level size/invalidation effect once
  // per retained source receipt.  A service error leaves no completion receipt
  // for the failed component (or any later component), marks this diagnostic
  // session failed, and permanently rejects re-entry.
  void execute(IntroFadePicturePhaseOneServices services);

  [[nodiscard]] FirstCutFadePhaseOneSessionStage stage() const noexcept {
    return stage_;
  }
  [[nodiscard]] const std::vector<FirstCutFadePhaseOneReceipt>& receipts() const noexcept {
    return receipts_;
  }

private:
  friend class NormalIntroSceneSession;
  explicit FirstCutFadePhaseOneSession(IntroRuntime& runtime);

  IntroRuntime* runtime_{};
  std::vector<FirstCutFadePhaseOneReceipt> targets_;
  std::vector<FirstCutFadePhaseOneReceipt> receipts_;
  FirstCutFadePhaseOneSessionStage stage_{FirstCutFadePhaseOneSessionStage::cold};
};

} // namespace off::graphics
