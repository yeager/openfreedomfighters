#pragma once

#include <cstdint>
#include <memory>
#include <utility>

namespace off::graphics {

class FreshIntroCamera;
class OwnedNormalIntroScene;

} // namespace off::graphics

namespace off::platform {
class SdlActiveViewHost;
}

namespace off::graphics {

// A project-owned binding between the prepared first-cut camera and the
// lifetime that owns its exact FreshIntroCamera instance. Only
// OwnedNormalIntroScene can mint it; SDL may use it to register a view without
// accepting unrelated shared_ptr values from a caller.
class SelectedIntroCameraViewLease final {
public:
  SelectedIntroCameraViewLease(const SelectedIntroCameraViewLease&) = delete;
  SelectedIntroCameraViewLease& operator=(const SelectedIntroCameraViewLease&) = delete;
  SelectedIntroCameraViewLease(SelectedIntroCameraViewLease&&) noexcept = default;
  SelectedIntroCameraViewLease& operator=(SelectedIntroCameraViewLease&&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return scene_owner_ && camera_ && camera_identity_ != 0U;
  }

private:
  friend class OwnedNormalIntroScene;
  friend class off::platform::SdlActiveViewHost;

  SelectedIntroCameraViewLease(
      std::shared_ptr<const void> scene_owner,
      std::shared_ptr<const FreshIntroCamera> camera,
      std::uint64_t camera_identity) noexcept
      : scene_owner_(std::move(scene_owner)), camera_(std::move(camera)),
        camera_identity_(camera_identity) {}

  std::shared_ptr<const void> scene_owner_;
  std::shared_ptr<const FreshIntroCamera> camera_;
  std::uint64_t camera_identity_{};
};

} // namespace off::graphics
