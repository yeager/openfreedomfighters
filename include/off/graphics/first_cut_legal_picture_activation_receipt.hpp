#pragma once

#include <cstdint>
#include <utility>

namespace off::platform {
class FirstCutPictureFrame;
}

namespace off::graphics {

class IntroRuntime;
struct FirstCutLegalPictureActivationResult;

// A move-only witness that the retained, source-bound legal-picture activation
// completed. It carries no source identity, camera state, timeline value, or
// draw-record data. Only IntroRuntime can mint it; only frame assembly can
// consume it.
class FirstCutLegalPictureActivationReceipt final {
 public:
  FirstCutLegalPictureActivationReceipt(
      const FirstCutLegalPictureActivationReceipt&) = delete;
  FirstCutLegalPictureActivationReceipt& operator=(
      const FirstCutLegalPictureActivationReceipt&) = delete;
  FirstCutLegalPictureActivationReceipt(
      FirstCutLegalPictureActivationReceipt&& other) noexcept
      : valid_(std::exchange(other.valid_, false)) {}
  FirstCutLegalPictureActivationReceipt& operator=(
      FirstCutLegalPictureActivationReceipt&& other) noexcept {
    if (this != &other) valid_ = std::exchange(other.valid_, false);
    return *this;
  }

 private:
  friend class IntroRuntime;
  friend struct FirstCutLegalPictureActivationResult;
  friend class off::platform::FirstCutPictureFrame;

  FirstCutLegalPictureActivationReceipt() noexcept = default;
  [[nodiscard]] bool consume() noexcept { return std::exchange(valid_, false); }

  bool valid_{true};
};

}  // namespace off::graphics
